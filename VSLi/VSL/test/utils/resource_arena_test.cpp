#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/arena_resource.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    namespace s = vsl::storage;
    namespace l = vsl::lifetime;
    using enum s::ResourceKind;
    constexpr auto BUFFER_USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    constexpr auto IMAGE_USAGE = vsl::ImageType::TRANSFER_DST | vsl::ImageType::SAMPLED;
    constexpr std::array<t::ResourceRequest, 4> REQUESTS{{{{1}, BUFFER}, {{2}, IMAGE}, {{3}, SAMPLER}, {{4}, BUFFER}}};
    constexpr t::ResourcePlan<4> PLAN(REQUESTS, {t::LifetimeScope::FRAME, 1});
    const vsl::BufferCreateInfo BUFFER_INFO{16, BUFFER_USAGE};
    const vsl::ImageCreateInfo IMAGE_INFO{IMAGE_USAGE, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA};
    const vsl::SamplerCreateInfo SAMPLER_INFO{vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE};
    class ResourceArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(ResourceArenaMetadataTest, StoresOnlyOneTypedReferenceRegardlessOfResourceKindCount) {
        using Record = l::detail::ArenaResource;
        static_assert(sizeof(Record::Reference) <= 2 * sizeof(void*));
        static_assert(std::is_nothrow_copy_assignable_v<Record::Reference>);
        RecordProperty("record_bytes", static_cast<int>(sizeof(Record)));
        RecordProperty("reference_bytes", static_cast<int>(sizeof(Record::Reference)));
        Record record{{}};
        vsl::impl::BufferImpl buffer;
        vsl::impl::ImageImpl image;
        record.reference = &buffer;
        EXPECT_EQ(l::detail::BufferAccess::Impl(record.buffer()), &buffer);
        EXPECT_EQ(record.get<vsl::impl::ImageImpl>(), nullptr);
        record.reference = &image;
        EXPECT_EQ(record.get<vsl::impl::ImageImpl>(), &image);
        EXPECT_EQ(l::detail::BufferAccess::Impl(record.buffer()), nullptr);
        EXPECT_LE(sizeof(Record), sizeof(t::ResourcePlacement) + 2 * sizeof(void*) + 3 * sizeof(size_t));
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, ImportedShaderIsAFixedManagedParentWithoutAGpuUseState) {
        using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        constexpr std::array<t::ResourceRequest, 5> source_requests{{{{1}, SHADER}, {{2}, BUFFER}, {{3}, DESCRIPTOR_LAYOUT}, {{4}, DESCRIPTOR_POOL}, {{5}, DESCRIPTOR_SET}}};
        constexpr std::array<t::ResourceRequest, 2> target_requests{{{{1}, SHADER, true}, {{2}, BUFFER}}};
        constexpr t::ResourcePlan<5> source_plan(source_requests);
        constexpr t::ResourcePlan<2> target_plan(target_requests);
        const auto bytes = vsl_test::read_binary(vsl_test::shader_path("triangle.vert.spv"));
        std::vector<std::uint32_t> code(bytes.size() / sizeof(std::uint32_t));
        std::memcpy(code.data(), bytes.data(), bytes.size());
        const vsl::DescriptorLayoutCreateInfo layout{{Set::POINTS.begin(), Set::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1};
        const vsl::BufferCreateInfo buffer{4, vsl::MemoryType::UNIFORM_BUFFER};
        std::weak_ptr<void> a, b;
        {
            const std::array<l::ResourceInit, 5> inputs{vsl::ShaderCreateInfo{vsl::ShaderType::VERTEX, code}, buffer, layout, pool, l::DescriptorSetInit{{4}, {3}}};
            l::ResourceArena source(device, source_plan, inputs, l::DependencyPolicy::DYNAMIC);
            const std::array<l::ResourceInit, 2> imports{source.shader({1}), buffer};
            l::ResourceArena target(device, target_plan, imports, l::DependencyPolicy::DYNAMIC);
            a = source.owner();
            b = target.owner();
            EXPECT_TRUE(target.shader({1}).owner_ref().managed());
            auto set = t::TypedDescriptorSet<Set>::FromResource(device, source.descriptor_set({5}));
            set.write<0>(View::FromView(source.buffer({2}), 1));
            EXPECT_THROW(set.write<0>(View::FromView(target.buffer({2}), 1)), std::invalid_argument);
            EXPECT_NO_THROW(set.require_complete());
        }
        EXPECT_TRUE(a.expired());
        EXPECT_TRUE(b.expired());
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, MixedObjectsFollowThePlanAndShareOneOwnerWithoutSharingUseStates) {
        const std::array<l::ResourceInit, 4> inputs{BUFFER_INFO, IMAGE_INFO, SAMPLER_INFO, BUFFER_INFO};
        l::ResourceArena arena(device, PLAN, inputs);
        EXPECT_EQ(arena.size(), 4u);
        EXPECT_EQ(arena.slots(), 4u);
        EXPECT_EQ(arena.lifetime(), PLAN.lifetime());
        const auto first = arena.borrow_buffer({1}), second = arena.borrow_buffer({4});
        const auto image = arena.image({2});
        const auto sampler = arena.sampler({3});
        const auto base = reinterpret_cast<std::uintptr_t>(l::detail::BufferAccess::Impl(first));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(l::detail::BufferAccess::Impl(second)) - base, PLAN.placement({4}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(image.data_.get()) - base, PLAN.placement({2}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(sampler.data_.get()) - base, PLAN.placement({3}).offset);
        EXPECT_NE(&first.use(), &second.use());
        EXPECT_NE(&first.use(), &image.use_state());
        EXPECT_NE(&image.use_state(), &sampler.use_state());
        EXPECT_FALSE(image.data_.owner_before(sampler.data_));
        EXPECT_FALSE(sampler.data_.owner_before(image.data_));
        EXPECT_THROW(arena.image({1}), std::invalid_argument);
        EXPECT_THROW(arena.buffer({404}), std::out_of_range);
        const std::array<std::uint32_t, 4> values{3, 5, 7, 11};
        arena.buffer({1}).write(std::as_bytes(std::span(values)));
        vsl::Buffer<BUFFER_USAGE, vsl::MemoryProperty::HOST_VISIBLE> output(device, sizeof(values));
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::CopyBuffer(arena.buffer({1}), vsl::BufferView::FromBuffer(output), sizeof(values), 0, 0, true);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 4> result{};
        output.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, values);
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, FailureRollsBackMixedSlotsAndAllowsTheSameRegionToBeReused) {
        s::ArenaMemory memory(PLAN.bytes(), PLAN.alignment());
        const std::array<l::ResourceInit, 4> valid{BUFFER_INFO, IMAGE_INFO, SAMPLER_INFO, BUFFER_INFO};
        auto bad = valid;
        auto invalid = BUFFER_INFO;
        invalid.properties = vsl::MemoryProperty::HOST_VISIBLE | static_cast<vsl::MemoryProperty>(0x80000000u);
        bad[3] = invalid;
        EXPECT_THROW((l::ResourceArena(device, PLAN, bad, memory)), vsl::exceptions::RuntimeException);
        EXPECT_NO_THROW((l::ResourceArena(device, PLAN, valid, memory)));
        auto invalid_image = IMAGE_INFO;
        invalid_image.width = 0;
        bad = valid;
        bad[1] = invalid_image;
        EXPECT_THROW((l::ResourceArena(device, PLAN, bad, memory)), std::invalid_argument);
        EXPECT_NO_THROW((l::ResourceArena(device, PLAN, valid, memory)));
        EXPECT_THROW((l::ResourceArena(device, PLAN, std::span(valid).first(3), memory)), std::invalid_argument);
        bad = valid;
        bad[1] = SAMPLER_INFO;
        EXPECT_THROW((l::ResourceArena(device, PLAN, bad, memory)), std::invalid_argument);
        EXPECT_NO_THROW((l::ResourceArena(device, PLAN, valid, memory)));
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, FailedClaimDoesNotReleaseAnotherArenaAndViewsDelayReuse) {
        const std::array<l::ResourceInit, 4> inputs{BUFFER_INFO, IMAGE_INFO, SAMPLER_INFO, BUFFER_INFO};
        s::ArenaMemory memory(PLAN.bytes(), PLAN.alignment());
        std::optional<vsl::ImageAccessor> retained;
        {
            l::ResourceArena first(device, PLAN, inputs, memory);
            EXPECT_THROW((l::ResourceArena(device, PLAN, inputs, memory)), std::invalid_argument);
            EXPECT_THROW((l::ResourceArena(device, PLAN, inputs, memory)), std::invalid_argument);
            retained.emplace(first.image({2}));
        }
        EXPECT_THROW((l::ResourceArena(device, PLAN, inputs, memory)), std::invalid_argument);
        retained.reset();
        EXPECT_NO_THROW((l::ResourceArena(device, PLAN, inputs, memory)));
        s::ArenaMemory small(PLAN.bytes() - 1, PLAN.alignment()), weak(PLAN.bytes(), 1);
        EXPECT_THROW((l::ResourceArena(device, PLAN, inputs, small)), std::invalid_argument);
        EXPECT_THROW((l::ResourceArena(device, PLAN, inputs, weak)), std::invalid_argument);
        auto destination = std::move(memory);
        EXPECT_THROW((l::ResourceArena(device, PLAN, inputs, memory)), std::invalid_argument);
        EXPECT_NO_THROW((l::ResourceArena(device, PLAN, inputs, destination)));
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, ExternalViewsPreserveRangesOwnersAndStatesWithoutPinningTheImportArena) {
        const std::array<l::ResourceInit, 4> source_inputs{BUFFER_INFO, IMAGE_INFO, SAMPLER_INFO, BUFFER_INFO};
        l::ResourceArena source(device, PLAN, source_inputs);
        const std::array<t::ResourceRequest, 4> requests{{{{10}, IMAGE, true}, {{11}, BUFFER}, {{12}, BUFFER, true}, {{13}, SAMPLER, true}}};
        const t::ResourcePlan<4> plan(requests);
        const std::array<l::ResourceInit, 4> inputs{source.image({2}), BUFFER_INFO, source.buffer({1}).view(4, 8), source.sampler({3})};
        std::weak_ptr<void> source_owner = source.owner(), import_owner;
        const auto baseline = source_owner.use_count();
        std::optional<vsl::BufferView> retained;
        {
            l::ResourceArena imported(device, plan, inputs);
            import_owner = imported.owner();
            EXPECT_EQ(source_owner.use_count(), baseline + 1);
            EXPECT_EQ(imported.slots(), 1u);
            EXPECT_EQ(imported.buffer({12}).byte_offset(), 4u);
            EXPECT_EQ(imported.buffer({12}).size(), 8u);
            EXPECT_THROW(imported.borrow_buffer({12}), std::logic_error);
            EXPECT_EQ(imported.buffer({12}).resource_use().state, source.buffer({1}).resource_use().state);
            EXPECT_EQ(&imported.image({10}).use_state(), &source.image({2}).use_state());
            EXPECT_EQ(&imported.sampler({13}).use_state(), &source.sampler({3}).use_state());
            EXPECT_FALSE(imported.image({10}).data_.owner_before(source_owner));
            EXPECT_FALSE(source_owner.owner_before(imported.image({10}).data_));
            retained.emplace(imported.buffer({12}));
        }
        EXPECT_TRUE(import_owner.expired());
        EXPECT_NO_THROW(retained->write(std::array<std::byte, 8>{}));
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, EmptyExternalOnlyAndMovedFromArenasHaveDefinedBehavior) {
        const t::ResourcePlan<0> empty(std::span<const t::ResourceRequest>{});
        l::ResourceArena original(device, empty, {});
        auto moved = std::move(original);
        EXPECT_EQ(moved.size(), 0u);
        EXPECT_EQ(moved.slots(), 0u);
        EXPECT_EQ(original.size(), 0u);
        EXPECT_THROW(original.buffer({1}), std::logic_error);
        const std::array<t::ResourceRequest, 1> external{{{{1}, IMAGE, true}}};
        const t::ResourcePlan<1> plan(external);
        std::array<l::ResourceInit, 1> inputs{vsl::ImageAccessor{}};
        EXPECT_THROW((l::ResourceArena(device, plan, inputs)), std::invalid_argument);
        vsl::Image<IMAGE_USAGE> image(device, 1, 1, IMAGE_INFO.format);
        inputs[0] = vsl::ImageAccessor{std::shared_ptr<vsl::impl::ImageImpl>(std::shared_ptr<void>{}, image.data_.get())};
        EXPECT_THROW((l::ResourceArena(device, plan, inputs)), std::invalid_argument);
        inputs[0] = image;
        l::ResourceArena imported(device, plan, inputs);
        EXPECT_EQ(imported.slots(), 0u);
        EXPECT_EQ(imported.image({1}).data_, image.data_);
        EXPECT_THROW((l::ResourceArena({}, plan, inputs)), std::invalid_argument);
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, UnconstructedExternalAccessorsAreRejectedBeforeDereferencingTheirDevice) {
        const std::array<t::ResourceRequest, 1> image_request{{{{1}, IMAGE, true}}}, sampler_request{{{{1}, SAMPLER, true}}};
        const t::ResourcePlan<1> image_plan(image_request), sampler_plan(sampler_request);
        const std::array<l::ResourceInit, 1> image{vsl::ImageAccessor{std::make_shared<vsl::impl::ImageImpl>()}};
        const std::array<l::ResourceInit, 1> sampler{vsl::SamplerAccessor{std::make_shared<vsl::impl::SamplerImpl>()}};
        EXPECT_THROW((l::ResourceArena(device, image_plan, image)), std::invalid_argument);
        EXPECT_THROW((l::ResourceArena(device, sampler_plan, sampler)), std::invalid_argument);
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, DeviceLocalBufferUsesTheSameTransferBackendAsOrdinaryBuffers) {
        const std::array<t::ResourceRequest, 1> requests{{{{1}, BUFFER}}};
        const t::ResourcePlan<1> plan(requests);
        const std::array<l::ResourceInit, 1> inputs{vsl::BufferCreateInfo{16, BUFFER_USAGE, vsl::MemoryProperty::DEVICE_LOCAL}};
        l::ResourceArena arena(device, plan, inputs);
        EXPECT_TRUE(vsl::contain(arena.borrow_buffer({1}).properties(), vsl::MemoryProperty::DEVICE_LOCAL));
        vsl::Buffer<BUFFER_USAGE, vsl::MemoryProperty::HOST_VISIBLE> input(device, 16), output(device, 16);
        const std::array<std::uint32_t, 4> values{17, 19, 23, 29};
        vsl::BufferView::FromBuffer(input).write(std::as_bytes(std::span(values)));
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::CopyBuffer(vsl::BufferView::FromBuffer(input), arena.buffer({1}), 16)
              << vsl::command::CopyBuffer(arena.buffer({1}), vsl::BufferView::FromBuffer(output), 16, 0, 0, true);
        EXPECT_TRUE(arena.borrow_buffer({1}).use().busy());
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 4> result{};
        output.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, values);
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, TypedComputeUsesPlacedAssetsAfterWrappersDieAndReclaimsThemOnCompletion) {
        using Set = t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                     t::DescriptorBinding<1, vsl::graphic_resource::Type::SAMPLED_IMAGE, vsl::ShaderFlag::COMPUTE>,
                                     t::DescriptorBinding<2, vsl::graphic_resource::Type::SAMPLER, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("reflection_separate_images.comp.spv"));
        t::TypedComputePipeline<t::ResourceSchema<Set>> pipeline(device, vsl::pipeline_layout::ShaderGroup("arena assets", {shader}));
        t::TypedStorageBuffer<std::uint32_t> output(device, std::in_place, 2);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        std::weak_ptr<void> owner;
        {
            auto command = [&] {
                const std::array<l::ResourceInit, 4> inputs{BUFFER_INFO, IMAGE_INFO, SAMPLER_INFO, BUFFER_INFO};
                l::ResourceArena arena(device, PLAN, inputs);
                owner = arena.owner();
                {
                    const std::array<std::uint8_t, 4> color{37, 83, 0, 255};
                    vsl::StagingBuffer<> pixels(device, color);
                    auto upload = make_manager();
                    auto initialization = upload.start_phase<vsl::ComputePhase>();
                    initialization << vsl::command::ImageUpload(&pixels, arena.image({2}), vsl::ImageLayout::UNDEFINED);
                    if (initialization.submit() != vsl::ComputePhase::SubmitResult::SUCCESS)
                        throw std::runtime_error("Arena image initialization submission failed");
                    if (!initialization.completion().wait())
                        throw std::runtime_error("Arena image initialization did not complete");
                }
                t::TypedDescriptorSet<Set> set(device, descriptors, vsl::ResourceRetention::COMPLETION);
                set.write<0>(output);
                set.write<1>(t::TypedSampledImage(arena.image({2})));
                set.write<2>(t::TypedSampler(arena.sampler({3})));
                return pipeline.bind_resources(set).dispatch(1);
            }();
            EXPECT_FALSE(owner.expired());
            phase << command;
        }
        EXPECT_FALSE(owner.expired());
        const vk::MemoryBarrier host_visible{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        auto buffer = manager.get_buffer();
        CommandAccess::Data(buffer)->command_buffers.at(buffer.get_current_buffer_idx()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, host_visible, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(owner.expired());
        std::array<std::uint32_t, 2> actual{};
        output.backend().read_into(std::as_writable_bytes(std::span(actual)));
        EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{37, 83}));
    }
    TEST_F(ResourceArenaVulkanIntegrationTest, CancellationReleasesOneArenaPinButRegistersEveryPhysicalResource) {
        using Set = t::DescriptorSet<0, t::TextureBinding<0, vsl::ShaderFlag::COMPUTE>>;
        vsl::GraphicResourceManager descriptors(device);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        std::weak_ptr<void> owner;
        {
            const std::array<l::ResourceInit, 4> inputs{BUFFER_INFO, IMAGE_INFO, SAMPLER_INFO, BUFFER_INFO};
            l::ResourceArena arena(device, PLAN, inputs);
            owner = arena.owner();
            t::TypedDescriptorSet<Set> set(device, descriptors, vsl::ResourceRetention::COMPLETION);
            set.write<0>(t::TypedTexture(arena.image({2}), arena.sampler({3})));
            set.resource_use()->record(manager.get_buffer());
            EXPECT_TRUE(arena.image({2}).use_state().busy());
            EXPECT_TRUE(arena.sampler({3}).use_state().busy());
            EXPECT_FALSE(arena.borrow_buffer({1}).use().busy());
            EXPECT_EQ(phase.completion().data_->work.lock()->completion_resources.size(), 2u);
        }
        EXPECT_FALSE(owner.expired());
        phase.cancel();
        EXPECT_TRUE(owner.expired());
    }
} // namespace
