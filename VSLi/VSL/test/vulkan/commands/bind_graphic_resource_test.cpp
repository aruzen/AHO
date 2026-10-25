#include "support/compute_fixture.hpp"
namespace {
    using BindGraphicResourceVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(BindGraphicResourceVulkanIntegrationTest, SingleAndVectorResourcesBindToComputeShader) {
        vsl::command::BindGraphicResource command(resource, vsl::graphic_resource::BindingDestination::COMPUTE);
        ASSERT_EQ(command.resources.size(), 1u);
        EXPECT_EQ(command.resources.front().data_, resource.data_);
        EXPECT_EQ(command.destination, vsl::graphic_resource::BindingDestination::COMPUTE);
        EXPECT_EQ(command.first_binding, 0u);
        EXPECT_FALSE(command.pipeline.has_value());
        command.set_pipeline(*pipeline);
        ASSERT_TRUE(command.pipeline.has_value());
        EXPECT_EQ(command.pipeline->data_, pipeline->data_);
        ExpectSequence(execute(13), 13);
        ExpectSequence(execute(29, true), 29);
    }
} // namespace

namespace {
    class DescriptorResourceLifetimeVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool DescriptorResourceLifetimeVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, ExplicitPipelineBindsAcrossSeparateStreamExpressions) {
        expect_validation_abort("");
        for (const bool vector_resources : {false, true}) {
            for (const bool shared_command : {false, true}) {
                auto manager = make_manager();
                auto phase = manager.start_phase<vsl::ComputePhase>();
                phase << *pipeline;
                auto command = std::make_shared<vsl::command::BindGraphicResource>(
                    vector_resources ? vsl::command::BindGraphicResource(std::vector{resource}, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline)
                                     : vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline));
                if (shared_command)
                    phase << std::static_pointer_cast<vsl::command::Command>(command);
                else
                    phase << *command;
                std::uint32_t base = 503 + 10 * vector_resources + shared_command;
                phase << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                      << vsl::command::Dispatch(COUNT);
                vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                          .dstAccessMask = vk::AccessFlagBits::eHostRead};
                auto buffer = manager.get_buffer();
                buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(phase.completion().wait());
                auto mapped = output->data();
                std::array<std::uint32_t, COUNT> actual;
                std::memcpy(actual.data(), mapped.data, sizeof(actual));
                ExpectSequence(actual, base);
            }
        }
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, MissingContextRestoresTheConstructorPipelineWithoutKeepingImplicitState) {
        vsl::ComputePipeline other(*layout);
        vsl::command::BindGraphicResource explicit_command(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline);
        explicit_command.set_pipeline(other);
        ASSERT_TRUE(explicit_command.pipeline);
        EXPECT_EQ(explicit_command.pipeline->data_, other.data_);
        explicit_command.set_pipeline(std::nullopt);
        ASSERT_TRUE(explicit_command.pipeline);
        EXPECT_EQ(explicit_command.pipeline->data_, pipeline->data_);
        vsl::command::BindGraphicResource implicit_command(resource, vsl::graphic_resource::BindingDestination::COMPUTE);
        implicit_command.set_pipeline(other);
        ASSERT_TRUE(implicit_command.pipeline);
        EXPECT_EQ(implicit_command.pipeline->data_, other.data_);
        implicit_command.set_pipeline(std::nullopt);
        EXPECT_FALSE(implicit_command.pipeline);
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, EmptyBindingPreservesThePreviouslyBoundDescriptorSets) {
        // 検証エラーはテストを失敗させ、不正なコマンドをdriverへ渡さない。
        expect_validation_abort("");
        auto manager = make_manager();
        const std::uint32_t base = 421;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << *pipeline
                  << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                  << vsl::command::BindGraphicResource(std::vector<vsl::GraphicResource>{}, vsl::graphic_resource::BindingDestination::COMPUTE)
                  << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                  << vsl::command::Dispatch(COUNT);
            const auto buffer = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                      .dstAccessMask = vk::AccessFlagBits::eHostRead};
            buffer.data_->command_buffers[buffer.data_->current_buffer_idx].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto mapped = output->data();
        std::array<std::uint32_t, COUNT> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        ExpectSequence(actual, base);
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, DispatchKeepsDescriptorBufferAndPipelineAliveUntilCompletion) {
        auto manager = make_manager();
        vsl::Completion completion;
        std::weak_ptr<vsl::impl::BufferImpl> weak_output = output->data_;
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_resource = resource.data_;
        std::weak_ptr<vsl::impl::PipelineImpl> weak_pipeline = pipeline->data_;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                auto stream = phase << *pipeline;
                std::uint32_t base = 307;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                       << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                       << vsl::command::Dispatch(COUNT);
            }
            resource = {};
            pipeline.reset();
            layout.reset();
            binding.reset();
            resources.reset();
            pool.data_.reset();
            output.reset();
            EXPECT_FALSE(weak_resource.expired());
            EXPECT_FALSE(weak_pipeline.expired());
            auto buffer = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                      .dstAccessMask = vk::AccessFlagBits::eHostRead};
            buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        ASSERT_TRUE(completion.wait());
        auto output_owner = weak_output.lock();
        ASSERT_TRUE(output_owner);
        // holderで寿命を延ばさず、Completionが保持する既存のマップから結果を読む。
        ASSERT_TRUE(vsl::contain(output_owner->properties, vsl::MemoryProperty::HOST_COHERENT));
        const auto* values = static_cast<const std::uint32_t*>(output_owner->mapped);
        ASSERT_NE(values, nullptr);
        for (std::size_t i = 0; i < COUNT; ++i)
            EXPECT_EQ(values[i], 307u + i);
        output_owner.reset();
        completion = {};
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak_resource.expired());
        EXPECT_TRUE(weak_pipeline.expired());
        EXPECT_TRUE(weak_output.expired());
    }
} // namespace
