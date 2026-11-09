#include <VSL/utils/typed_pipeline.hpp>
#include <VSL/utils/typed_batch.hpp>
#include "support/render_fixture.hpp"
#include <VSL/operation/submission_ops.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/submission_retention.hpp>
#include <algorithm>

namespace {
    namespace t = vsl::typed;
    struct Position {
        float xy[2];
    };
    struct Color {
        float rgb[3];
    };
    using Positions = t::VertexRecord<Position, VSL_VERTEX_MEMBER(Position, xy)>;
    using Colors = t::VertexRecord<Color, VSL_VERTEX_MEMBER(Color, rgb)>;
    using Input = t::TypedVertexInput<Positions::Binding<0>, Colors::Binding<1, t::VertexTiming::NEXT_INSTANCE, 1>>;
    const std::array<Position, 3> POSITIONS{{{{-1, -1}}, {{3, -1}}, {{-1, 3}}}};
    const std::array<Color, 2> COLORS{{{{1, 0, 0}}, {{.25f, .5f, .75f}}}};
    class TypedIndexVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        t::TypedGraphicsPipeline<Input> make_typed();
    };
    struct UploadInputProbe {
        std::span<std::uint16_t> values;
        unsigned calls = 0;
        static VkResult Submit(void* context, VkQueue queue, const VkSubmitInfo* info, VkFence fence) noexcept;
    };
    struct UploadFailureProbe {
        VkResult submit_result = VK_SUCCESS;
        bool fence_error = false;
        unsigned submissions = 0, waits = 0, idles = 0;
        static VkResult Submit(void*, VkQueue, const VkSubmitInfo*, VkFence) noexcept;
        static VkResult Status(void*, VkDevice, VkFence) noexcept;
        static VkResult Wait(void*, VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) noexcept;
        static VkResult Idle(void*, VkDevice) noexcept;
        vsl::operation::SubmissionNativeCalls calls() { return {this, Idle, Wait, Submit, Status}; }
    };
    VkResult UploadInputProbe::Submit(void* context, VkQueue queue, const VkSubmitInfo* info, VkFence fence) noexcept {
        auto& probe = *static_cast<UploadInputProbe*>(context);
        const auto result = vkQueueSubmit(queue, 1, info, fence);
        if (++probe.calls == 1 && result == VK_SUCCESS)
            std::ranges::fill(probe.values, std::uint16_t{99});
        return result;
    }
    VkResult UploadFailureProbe::Submit(void* context, VkQueue queue, const VkSubmitInfo* info, VkFence fence) noexcept {
        auto& probe = *static_cast<UploadFailureProbe*>(context);
        ++probe.submissions;
        return probe.submit_result == VK_SUCCESS ? vkQueueSubmit(queue, 1, info, fence) : probe.submit_result;
    }
    VkResult UploadFailureProbe::Status(void* context, VkDevice device, VkFence fence) noexcept {
        const auto& probe = *static_cast<UploadFailureProbe*>(context);
        return probe.fence_error || probe.submit_result != VK_SUCCESS ? VK_NOT_READY : vkGetFenceStatus(device, fence);
    }
    VkResult UploadFailureProbe::Wait(void* context, VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) noexcept {
        auto& probe = *static_cast<UploadFailureProbe*>(context);
        ++probe.waits;
        return probe.fence_error ? VK_ERROR_OUT_OF_HOST_MEMORY : vkWaitForFences(device, count, fences, all, timeout);
    }
    VkResult UploadFailureProbe::Idle(void* context, VkDevice device) noexcept {
        ++static_cast<UploadFailureProbe*>(context)->idles;
        return vkDeviceWaitIdle(device);
    }
    t::TypedGraphicsPipeline<Input> TypedIndexVulkanIntegrationTest::make_typed() {
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path("vertices.vert.spv"));
        return t::TypedGraphicsPipeline<Input>(device, *pass, pl::ShaderGroup("indexed", {shader, *fragment}),
                                               pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                                               pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
    }
    TEST(TypedIndexTest, NativeDrawRejectsNarrowingInEveryArgument) {
        using Draw = vsl::command::DrawIndexed;
        const auto overflow = size_t(UINT32_MAX) + 1;
        EXPECT_THROW(Draw(overflow, 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, overflow), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, overflow), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, 0, std::int64_t(INT32_MAX) + 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, 0, std::int64_t(INT32_MIN) - 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Draw(3, 1, 0, 0, overflow), vsl::exceptions::RuntimeException);
    }
    TEST_F(TypedIndexVulkanIntegrationTest, Uint16DrawUsesFirstIndexNegativeBaseVertexAndFirstInstance) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 5> values{99, 1, 2, 3, 99};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 1, -1, 1); }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, Uint32DeviceLocalIndicesAboveUint16RangeArePreserved) {
        auto typed = make_typed();
        auto verify = [&](auto policy) {
            t::TypedVertexBuffer<Position> positions(device, POSITIONS, policy);
            t::TypedVertexBuffer<Color> colors(device, COLORS, policy);
            const std::array<std::uint32_t, 3> values{65536, 65537, 65538};
            auto manager = make_manager();
            t::TypedIndexBuffer<std::uint32_t, vsl::MemoryProperty::DEVICE_LOCAL> indices(device, manager, values, policy);
            ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, -65536, 1); }));
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
    }
    TEST_F(TypedIndexVulkanIntegrationTest, OddUint16UploadUsesOneSnapshotAndPreservesTheSourceManagersRecording) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        auto verify = [&](auto policy) {
            auto manager = make_manager().independent(2);
            auto original = manager.start_phase<vsl::ComputePhase>();
            auto buffer = manager.get_buffer();
            const auto slot = buffer.get_current_buffer_idx();
            const auto work = buffer.data_->recording_context->work;
            std::array<std::uint16_t, 3> values{0, 1, 2};
            UploadInputProbe probe{values};
            const vsl::operation::ScopedSubmissionNativeCalls native({.context = &probe, .queue_submit = UploadInputProbe::Submit});
            t::TypedIndexBuffer<std::uint16_t, vsl::MemoryProperty::DEVICE_LOCAL> indices(device, manager, values, policy);
            EXPECT_EQ(probe.calls, 1u);
            EXPECT_EQ(values, (std::array<std::uint16_t, 3>{99, 99, 99}));
            EXPECT_EQ(indices.bounds(0, 3), (std::pair<std::uint32_t, std::uint32_t>{0, 2}));
            EXPECT_EQ(buffer.get_current_buffer_idx(), slot);
            EXPECT_EQ(buffer.data_->recording_context->work, work);
            EXPECT_TRUE(buffer.data_->recording_context->recording);
            EXPECT_EQ(original.completion().status(), vsl::Completion::Status::PENDING);
            ExpectTriangleColor(render([&](auto& stream) { stream << typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, 0, 1); }));
            original.cancel();
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
    }
    TEST_F(TypedIndexVulkanIntegrationTest, InvalidIndexVertexAndInstanceRangesAreRejectedBeforeRecording) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        auto binding = typed.bind_vertices(positions, colors);
        EXPECT_THROW(binding.draw_indexed(indices, 4), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 1, 1, 3), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 3, 1, 0, -1), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 3, 1, 0, 1), std::out_of_range);
        EXPECT_THROW(binding.draw_indexed(indices, 3, 2, 0, 0, 1), std::out_of_range);
        EXPECT_NO_THROW(binding.draw_indexed(indices, 0, 0, indices.size()));
        const std::array<std::uint16_t, 3> changed{0, 1, 20};
        auto alias = indices;
        alias.copy(changed);
        EXPECT_THROW(binding.draw_indexed(indices, 3), std::out_of_range);
        EXPECT_THROW(indices.copy(std::span(values).first(2)), std::invalid_argument);
        EXPECT_THROW(vsl::command::BindIndexBufferAt(indices.backend(), vsl::command::IndexType::UINT16, 1), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }
    TEST_F(TypedIndexVulkanIntegrationTest, UploadSubmissionFailuresRollBackOrRemainOwnedUntilTheOriginalManagersIdleProof) {
        const std::array<std::uint32_t, 2> values{3, 7};
        auto verify = [&](auto policy, VkResult error) {
            auto manager = make_manager();
            const auto previous = vsl::lifetime::quarantined_submission_count();
            UploadFailureProbe probe;
            probe.submit_result = error;
            const vsl::operation::ScopedSubmissionNativeCalls native(probe.calls());
            using Buffer = t::TypedUniformBuffer<std::uint32_t, vsl::MemoryProperty::DEVICE_LOCAL>;
            EXPECT_THROW((Buffer(device, manager, values, policy)), vk::SystemError);
            EXPECT_EQ(probe.submissions, 1u);
            EXPECT_EQ(vsl::lifetime::quarantined_submission_count(), previous + (error == VK_ERROR_UNKNOWN ? 1 : 0));
            EXPECT_EQ(probe.idles, 0u);
            if (error == VK_ERROR_UNKNOWN) {
                EXPECT_NO_THROW(manager.wait_idle());
                EXPECT_EQ(probe.idles, 1u);
            }
            EXPECT_EQ(vsl::lifetime::quarantined_submission_count(), previous);
        };
        for (const auto error : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_UNKNOWN}) {
            verify(t::HeapStorage{}, error);
            verify(t::ArenaStorage<>{}, error);
        }
    }
    TEST_F(TypedIndexVulkanIntegrationTest, UploadFenceObservationFailureWaitsForIdleBeforePropagatingTheError) {
        const std::array<std::uint32_t, 2> values{3, 7};
        auto verify = [&](auto policy) {
            auto manager = make_manager();
            const auto previous = vsl::lifetime::quarantined_submission_count();
            UploadFailureProbe probe;
            probe.fence_error = true;
            const vsl::operation::ScopedSubmissionNativeCalls native(probe.calls());
            using Buffer = t::TypedUniformBuffer<std::uint32_t, vsl::MemoryProperty::DEVICE_LOCAL>;
            EXPECT_THROW((Buffer(device, manager, values, policy)), std::runtime_error);
            EXPECT_EQ(probe.submissions, 1u);
            EXPECT_EQ(probe.waits, 2u);
            EXPECT_EQ(probe.idles, 1u);
            EXPECT_EQ(vsl::lifetime::quarantined_submission_count(), previous);
        };
        verify(t::HeapStorage{});
        verify(t::ArenaStorage<>{});
    }
    TEST_F(TypedIndexVulkanIntegrationTest, CapturedCommandRejectsIndexUpdatesBeforeRecording) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 3> values{0, 1, 2}, changed{0, 2, 1};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        const auto binding = typed.bind_vertices(positions, colors);
        auto command = binding.draw_indexed(indices, 3, 1, 0, 0, 1);
        auto alias = indices;
        // 修正前の再現でも、driverへ渡すのは有効なindexだけにする。
        alias.copy(changed);
        ExpectTriangleColor(render([&](auto& stream) {
            EXPECT_THROW(stream << command, std::logic_error);
            EXPECT_FALSE(positions.resource_use().state->busy());
            EXPECT_FALSE(indices.resource_use().state->busy());
            stream << binding.draw_indexed(indices, 3, 1, 0, 0, 1);
        }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, BatchRejectsUpdatedIndicesBeforeAnyResourceIsUsed) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint32_t, 3> values{0, 1, 2}, changed{0, 2, 1};
        t::TypedIndexBuffer<std::uint32_t> first(device, values), second(device, values);
        const auto binding = typed.bind_vertices(positions, colors);
        auto batch = t::TypedBatch(binding.draw_indexed(first, 3, 1, 0, 0, 1), binding.draw_indexed(second, 3, 1, 0, 0, 1));
        second.copy(changed);
        ExpectTriangleColor(render([&](auto& stream) {
            EXPECT_THROW(stream << batch, std::logic_error);
            EXPECT_FALSE(first.resource_use().state->busy());
            EXPECT_FALSE(positions.resource_use().state->busy());
            stream << binding.draw_indexed(second, 3, 1, 0, 0, 1);
        }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, UnchangedAndFailedIndexCopiesKeepCapturedCommandsUsable) {
        auto typed = make_typed();
        t::TypedVertexBuffer<Position> positions(device, POSITIONS);
        t::TypedVertexBuffer<Color> colors(device, COLORS);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        auto command = typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, 0, 1);
        ExpectTriangleColor(render([&](auto& stream) { stream << command; }));
        EXPECT_THROW(indices.copy(std::span(values).first(2)), std::invalid_argument);
        ExpectTriangleColor(render([&](auto& stream) { stream << command; }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, CommandsKeepIndexAndVertexOwnersAfterAllWrappersAreDestroyed) {
        std::weak_ptr<void> owner;
        auto command = [&] {
            auto typed = make_typed();
            t::TypedVertexBuffer<Position> positions(device, POSITIONS);
            t::TypedVertexBuffer<Color> colors(device, COLORS);
            const std::array<std::uint16_t, 3> values{0, 1, 2};
            t::TypedIndexBuffer<std::uint16_t> indices(device, values);
            owner = indices.storage_view().owner();
            return typed.bind_vertices(positions, colors).draw_indexed(indices, 3, 1, 0, 0, 1);
        }();
        std::optional owned(std::move(command));
        ExpectTriangleColor(render([&](auto& stream) { stream << *owned; owned.reset(); EXPECT_FALSE(owner.expired()); }));
    }
    TEST_F(TypedIndexVulkanIntegrationTest, IndexedDrawKeepsDescriptorAndPushContracts) {
        struct Tint {
            float rgba[4];
        };
        using TintLayout = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
        using Set = t::DescriptorSet<1, t::UniformBinding<4, TintLayout, vsl::ShaderFlag::VERTEX>>;
        using Resources = t::ResourceSchema<Set>;
        using Push = t::PushSchema<t::PushRange<TintLayout, vsl::ShaderFlag::VERTEX, 16>>;
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path("typed_push_resources.vert.spv"));
        t::TypedGraphicsPipeline<t::TypedVertexInput<>, Resources, Push> typed(device, *pass,
                                                                               pl::ShaderGroup("indexed resources", {shader, *fragment}), pl::ColorBlend{}, pl::InputAssembly{},
                                                                               pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        const Tint bias{{.125f, .25f, .375f, 0}};
        t::TypedUniformBuffer<TintLayout> uniform(device, bias);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Set::Layout> set(device, manager);
        set.write<4>(uniform);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << typed.bind_vertices().push_constants(Push::Values(bias)).bind_resources(set).draw_indexed(indices, 3);
        }));
    }
} // namespace
