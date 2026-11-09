#include <AHO/resource/imported_buffer.hpp>
#include <AHO/resource/typed_buffer.hpp>
#include <AHO/command/buffer_upload.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <array>

namespace {
    template<typename T>
    concept has_reserve = requires(T& value) { value.reserve(1); };
    static_assert(!has_reserve<aho::resource::ImportedBuffer>);
    static_assert(aho::resource::gpu_buffer<aho::resource::ImportedBuffer>);
} // namespace
#ifdef AHO_GPU_TESTS
#include "support/vulkan_fixture.hpp"
#include <VSL/utils/typed_compute.hpp>

namespace {
    namespace r = aho::resource;
    class ImportedBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ImportedBufferVulkanIntegrationTest, PartialImportOwnsItsRangeAndHasOnePhysicalUseState) {
        std::weak_ptr<void> owner;
        auto imported = [&] {
            const std::array<vsl::BufferCreateInfo, 2> infos{{{32, r::detail::STANDARD_BUFFER_USAGE}, {16, r::detail::STANDARD_BUFFER_USAGE}}};
            vsl::lifetime::BufferArena arena(device, infos);
            owner = arena.owner();
            arena.borrow(0).fill(std::byte{});
            return r::import_buffer(arena.view(0).view(8, 16));
        }();
        EXPECT_FALSE(owner.expired());
        EXPECT_EQ(imported.capacity(), 16u);
        auto alias = imported;
        std::array<std::uint32_t, 2> values{7, 9}, result{};
        alias.assign(std::as_bytes(std::span(values)));
        EXPECT_EQ(imported.size(), 8u);
        imported.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, values);
        auto typed = r::storage_view<std::uint32_t>(imported);
        EXPECT_EQ(typed.byte_offset(), 8u);
        EXPECT_EQ(typed.descriptor().resource_use().state, imported.binding().resource_use().state);
        EXPECT_THROW(imported.binding().native(), std::logic_error);
        EXPECT_THROW(imported.resize(17), std::out_of_range);
        EXPECT_THROW(imported.write(std::as_bytes(std::span(values)), SIZE_MAX), std::out_of_range);
        imported.resize(0);
        EXPECT_THROW(r::vertex_view<std::uint32_t>(imported), std::out_of_range);
    }
    TEST_F(ImportedBufferVulkanIntegrationTest, PartialArenaAndHeapTransfersPreserveOffsetsAndCancellationReleasesOwners) {
        auto manager = make_manager();
        std::weak_ptr<void> owner;
        vsl::Completion completion;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                const vsl::BufferCreateInfo info{32, r::detail::STANDARD_BUFFER_USAGE};
                vsl::lifetime::BufferArena arena(device, std::span(&info, 1));
                owner = arena.owner();
                auto first = r::import_buffer(arena.view(0).view(4, 8));
                auto second = r::import_buffer(arena.view(0).view(16, 8));
                r::Buffer<r::BufferType::GPU_FRIENDLY> heap(device, 8);
                const std::array<std::uint32_t, 2> values{21, 34};
                heap.write(std::as_bytes(std::span(values)));
                phase << first.upload(heap) << second.upload(first);
                EXPECT_TRUE(first.busy());
                EXPECT_TRUE(second.busy());
                EXPECT_THROW(first.write({}), std::logic_error);
            }
            EXPECT_FALSE(owner.expired());
            phase.cancel();
            EXPECT_TRUE(owner.expired());
        }
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
    }
    TEST_F(ImportedBufferVulkanIntegrationTest, HeapToArenaToHeapGpuCopyReturnsTheOriginalData) {
        auto manager = make_manager();
        const vsl::BufferCreateInfo info{32, r::detail::STANDARD_BUFFER_USAGE};
        vsl::lifetime::BufferArena arena(device, std::span(&info, 1));
        auto imported = r::import_buffer(arena.view(0).view(8, 16));
        r::Buffer<r::BufferType::GPU_FRIENDLY> heap(device, 16), output(device, 16);
        const std::array<std::uint32_t, 4> values{2, 3, 5, 7};
        heap.write(std::as_bytes(std::span(values)));
        r::Buffer<r::BufferType::GENERAL> cpu;
        cpu.assign(std::as_bytes(std::span(values)));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << imported.upload(cpu) << imported.upload(heap) << output.upload(imported);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 4> result{};
        output.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, values);
        EXPECT_THROW(imported.binding().native(), std::logic_error);
    }

    TEST_F(ImportedBufferVulkanIntegrationTest, TypedCommandKeepsPartialImportedStorageThroughRecordingAndCompletion) {
        namespace t = vsl::typed;
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>, t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<t::ResourceSchema<Set>> pipeline(device, vsl::pipeline_layout::ShaderGroup("imported arena", {shader}));
        auto manager = make_manager();
        r::Buffer<r::BufferType::GPU_FRIENDLY> readback(device, 4);
        std::weak_ptr<void> owner;
        vsl::Completion completed;
        {
            std::optional<r::BufferBinding> output;
            auto command = [&] {
                const std::array<vsl::BufferCreateInfo, 2> infos{{{4, r::detail::STANDARD_BUFFER_USAGE}, {512, r::detail::STANDARD_BUFFER_USAGE}}};
                vsl::lifetime::BufferArena arena(device, infos);
                owner = arena.owner();
                auto input = r::import_buffer(arena.view(0));
                auto result = r::import_buffer(arena.view(1).view(256, 4));
                const std::uint32_t value = 53;
                input.write(std::as_bytes(std::span(&value, 1)));
                output.emplace(result.binding());
                t::TypedDescriptorSet<Set> set(device, descriptors, vsl::ResourceRetention::COMPLETION);
                set.write<0>(r::uniform_view<std::uint32_t>(input));
                set.write<1>(r::storage_view<std::uint32_t>(result));
                return pipeline.bind_resources(set).dispatch(1);
            }();
            auto phase = manager.start_phase<vsl::ComputePhase>();
            struct Source {
                r::BufferBinding value;
                r::BufferBinding binding() const { return value; }
            };
            phase << command << readback.upload(Source{*output});
            output.reset();
            EXPECT_FALSE(owner.expired());
            completed = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(completed.wait());
        }
        EXPECT_TRUE(owner.expired());
        std::uint32_t value = 0;
        readback.read_into(std::as_writable_bytes(std::span(&value, 1)));
        EXPECT_EQ(value, 53u);
    }
} // namespace
#endif
