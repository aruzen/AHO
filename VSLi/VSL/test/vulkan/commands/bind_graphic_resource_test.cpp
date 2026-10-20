#include "support/compute_fixture.hpp"
namespace {
    using BindGraphicResourceVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(BindGraphicResourceVulkanIntegrationTest, SingleAndVectorResourcesBindToComputeShader) {
        vsl::command::BindGraphicResource command(resource, vsl::graphic_resource::BindingDestination::Compute);
        ASSERT_EQ(command.resources.size(), 1u);
        EXPECT_EQ(command.resources.front()._data, resource._data);
        EXPECT_EQ(command.destination, vsl::graphic_resource::BindingDestination::Compute);
        EXPECT_EQ(command.first_binding, 0u);
        EXPECT_FALSE(command.pipeline.has_value());
        command.setPipeline(*pipeline);
        ASSERT_TRUE(command.pipeline.has_value());
        EXPECT_EQ(command.pipeline->_data, pipeline->_data);
        expect_sequence(execute(13), 13);
        expect_sequence(execute(29, true), 29);
    }
} // namespace

namespace {
    class DescriptorResourceLifetimeVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool DescriptorResourceLifetimeVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, DispatchKeepsDescriptorBufferAndPipelineAliveUntilCompletion) {
        auto manager = make_manager();
        vsl::Completion completion;
        std::weak_ptr<vsl::_impl::Buffer_impl> weak_output = output->_data;
        std::weak_ptr<vsl::_impl::GraphicResource_impl> weak_resource = resource._data;
        std::weak_ptr<vsl::_impl::Pipeline_impl> weak_pipeline = pipeline->_data;
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                auto stream = phase << *pipeline;
                std::uint32_t base = 307;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::Compute)
                       << vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, base)
                       << vsl::command::Dispatch(count);
            }
            resource = {};
            pipeline.reset();
            layout.reset();
            binding.reset();
            resources.reset();
            pool._data.reset();
            output.reset();
            EXPECT_FALSE(weak_resource.expired());
            EXPECT_FALSE(weak_pipeline.expired());
            auto buffer = manager.getBuffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                      .dstAccessMask = vk::AccessFlagBits::eHostRead};
            buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
        }
        ASSERT_TRUE(completion.wait());
        auto output_owner = weak_output.lock();
        ASSERT_TRUE(output_owner);
        // holderで寿命を延ばさず、Completionが保持する既存のマップから結果を読む。
        ASSERT_TRUE(vsl::contain(output_owner->properties, vsl::MemoryProperty::HostCoherent));
        const auto* values = static_cast<const std::uint32_t*>(output_owner->mapped);
        ASSERT_NE(values, nullptr);
        for (std::size_t i = 0; i < count; ++i)
            EXPECT_EQ(values[i], 307u + i);
        output_owner.reset();
        completion = {};
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak_resource.expired());
        EXPECT_TRUE(weak_pipeline.expired());
        EXPECT_TRUE(weak_output.expired());
    }
} // namespace
