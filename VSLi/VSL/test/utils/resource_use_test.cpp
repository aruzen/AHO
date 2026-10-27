#include <VSL/vulkan/resource_use.hpp>
#include <VSL/utils/typed_batch.hpp>
#include "support/render_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    class ResourceUseVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    class TrackedDrawVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    struct Vertex {
        float xy[2];
        float rgb[3];
    };
    using Record = t::VertexRecord<Vertex, VSL_VERTEX_MEMBER(Vertex, xy), VSL_VERTEX_MEMBER(Vertex, rgb)>;
    using Input = t::TypedVertexInput<Record::Binding<0>>;
    const std::array<Vertex, 3> VERTICES{{{{-1, -1}, {.25f, .5f, .75f}}, {{3, -1}, {.25f, .5f, .75f}}, {{-1, 3}, {.25f, .5f, .75f}}}};
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>, t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using Schema = t::ResourceSchema<Set>;

    TEST_F(ResourceUseVulkanIntegrationTest, MultipleRecordingPhasesBlockCpuWritesUntilEveryUseFinishes) {
        auto manager = make_manager().independent(1);
        auto other = manager.independent(1);
        vsl::ResourceUseState state;
        EXPECT_FALSE(state.busy());
        EXPECT_TRUE(state.wait(0));
        EXPECT_THROW(state.mark_used({}), std::logic_error);
        auto first = manager.start_phase<vsl::ComputePhase>();
        auto second = other.start_phase<vsl::ComputePhase>();
        state.mark_used(first.completion());
        state.mark_used(first.completion());
        state.mark_used(second.completion());
        EXPECT_TRUE(state.busy());
        EXPECT_FALSE(state.wait(0));
        EXPECT_THROW(state.require_idle(), std::logic_error);
        EXPECT_THROW(state.check_available(first.completion()), std::logic_error);
        ASSERT_EQ(second.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(second.completion().wait());
        EXPECT_TRUE(state.busy());
        first.cancel();
        EXPECT_TRUE(state.wait(0));
        EXPECT_FALSE(state.busy());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, SubmissionRetainsTheOwnerWithoutRetainingTheUseState) {
        auto manager = make_manager().independent(1);
        std::weak_ptr<int> owner;
        std::weak_ptr<vsl::ResourceUseState> state;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                auto value = std::make_shared<int>(9);
                auto tracking = std::make_shared<vsl::ResourceUseState>();
                owner = value;
                state = tracking;
                vsl::ResourceUse{tracking, value}.record(manager.get_buffer());
            }
            EXPECT_TRUE(state.expired());
            EXPECT_FALSE(owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(owner.expired());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, ExternalViewFactoriesValidateUsageAlignmentAndCapacity) {
        vsl::Buffer<vsl::MemoryType::VERTEX_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> vertex(device, 32);
        vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> uniform(device, 32);
        using VertexView = t::TypedVertexBufferView<float>;
        using UniformView = t::TypedResourceBufferView<float, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        EXPECT_THROW(VertexView::FromBuffer(uniform, 1), std::invalid_argument);
        EXPECT_THROW(UniformView::FromBuffer(vertex, 1), std::invalid_argument);
        EXPECT_THROW(VertexView::FromBuffer(vertex, 0), std::out_of_range);
        EXPECT_THROW(VertexView::FromBuffer(vertex, SIZE_MAX), std::out_of_range);
        EXPECT_THROW(VertexView::FromBuffer(vertex, 1, 1), std::invalid_argument);
        EXPECT_THROW(UniformView::FromBuffer(uniform, 1, SIZE_MAX), std::out_of_range);
        const auto view = UniformView::FromBuffer(uniform, 4, 8).view(1, 2);
        EXPECT_EQ(view.byte_offset(), 12u);
        EXPECT_EQ(view.size(), 2u);
    }

    TEST_F(ResourceUseVulkanIntegrationTest, RetainedFrameMayOwnItsResourceUseStateWithoutACycle) {
        struct Frame {
            vsl::ResourceUseState use;
            vsl::CompletionObservation update;
        };
        std::weak_ptr<Frame> owner;
        {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto frame = std::make_shared<Frame>();
            owner = frame;
            frame->use.mark_used(phase.completion());
            frame->update = vsl::CompletionObservation(phase.completion());
            phase.completion().retain(frame);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(owner.expired());
    }

    TEST_F(ResourceUseVulkanIntegrationTest, ObservationsPreserveCompletedAndCancelledResultsAfterOwnersAreDestroyed) {
        vsl::CompletionObservation complete, cancelled;
        {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            complete = vsl::CompletionObservation(phase.completion());
            EXPECT_EQ(complete.status(), vsl::Completion::Status::PENDING);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_EQ(complete.status(), vsl::Completion::Status::COMPLETE);
        {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            cancelled = vsl::CompletionObservation(phase.completion());
            phase.cancel();
        }
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
    }

    TEST_F(ResourceUseVulkanIntegrationTest, DescriptorUpdatesAfterBindingUseTheLatestResourcesAndFreezeDuringRecording) {
        auto manager = make_manager().independent(1);
        vsl::GraphicResourceManager descriptors(device);
        t::TypedDescriptorSet<Set> set(device, descriptors);
        using Native = vsl::Buffer<vsl::MemoryType::UNIFORM_BUFFER, vsl::MemoryProperty::HOST_VISIBLE>;
        const std::uint32_t a = 100, b = 200, zero = 0;
        Native first(device, &a), second(device, &b);
        auto old_use = std::make_shared<vsl::ResourceUseState>();
        auto new_use = std::make_shared<vsl::ResourceUseState>();
        using View = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        auto old_view = View::FromBuffer(first, 1, 0, {old_use, first.data_});
        auto new_view = View::FromBuffer(second, 1, 0, {new_use, second.data_});
        t::TypedStorageBuffer<std::uint32_t> output(device, zero);
        set.write<0>(old_view);
        set.write<1>(output);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        t::TypedComputePipeline<Schema> pipeline(device, vsl::pipeline_layout::ShaderGroup("tracked", {shader}));
        const auto command = pipeline.bind_resources(set).dispatch(1);
        set.write<0>(new_view);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << command;
        EXPECT_FALSE(old_use->busy());
        EXPECT_TRUE(new_use->busy());
        EXPECT_THROW(set.write<0>(old_view), std::logic_error);
        EXPECT_THROW(output.copy(std::span(&zero, 1)), std::logic_error);
        auto alias = set;
        EXPECT_THROW(alias.write<0>(old_view), std::logic_error);
        auto buffer = manager.get_buffer();
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        buffer.data_->command_buffers[0].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        auto mapped = output.backend().data();
        EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), b);
        EXPECT_NO_THROW(set.write<0>(old_view));
        EXPECT_FALSE(new_use->busy());
    }

    TEST_F(TrackedDrawVulkanIntegrationTest, BatchTracksEveryVertexOwnerEvenWhenItsBindIsSkipped) {
        vsl::Buffer<vsl::MemoryType::VERTEX_BUFFER, vsl::MemoryProperty::HOST_VISIBLE> native(device, VERTICES);
        auto first = std::make_shared<vsl::ResourceUseState>();
        auto second = std::make_shared<vsl::ResourceUseState>();
        using View = t::TypedVertexBufferView<Vertex>;
        const auto first_view = View::FromBuffer(native, 3, 0, {first, native.data_});
        const auto second_view = View::FromBuffer(native, 3, 0, {second, native.data_});
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("vertices.vert.spv"));
        t::TypedGraphicsPipeline<Input> pipeline(device, *pass, pl::ShaderGroup("tracked vertices", {source, *fragment}),
                                                 pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        auto batch = t::TypedBatch(pipeline.bind_vertices(first_view).draw(3), pipeline.bind_vertices(second_view).draw(3));
        EXPECT_EQ(batch.statistics().vertex_binds, 1u);
        ExpectTriangleColor(render([&](auto& stream) {
            stream << batch;
            EXPECT_TRUE(first->busy());
            EXPECT_TRUE(second->busy());
        }));
        EXPECT_FALSE(first->busy());
        EXPECT_FALSE(second->busy());
    }

    TEST_F(TrackedDrawVulkanIntegrationTest, IndexedDrawBlocksOwnedVertexAndIndexUpdatesUntilCancellation) {
        t::TypedVertexBuffer<Vertex> vertices(device, VERTICES);
        const std::array<std::uint16_t, 3> values{0, 1, 2};
        t::TypedIndexBuffer<std::uint16_t> indices(device, values);
        namespace pl = vsl::pipeline_layout;
        vsl::Shader<vsl::ShaderType::VERTEX> source(device, vsl_test::shader_path("vertices.vert.spv"));
        t::TypedGraphicsPipeline<Input> pipeline(device, *pass, pl::ShaderGroup("tracked indices", {source, *fragment}),
                                                 pl::ColorBlend{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::RenderPassBegin(*pass, *framebuffer) << vsl::Viewport(description) << vsl::Scissor(description)
              << pipeline.bind_vertices(vertices).draw_indexed(indices, 3) << vsl::command::RenderPassEnd{};
        EXPECT_THROW(vertices.copy(VERTICES), std::logic_error);
        EXPECT_THROW(indices.copy(values), std::logic_error);
        phase.cancel();
        EXPECT_NO_THROW(vertices.copy(VERTICES));
        EXPECT_NO_THROW(indices.copy(values));
    }
} // namespace
