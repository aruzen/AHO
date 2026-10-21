#include "support/render_fixture.hpp"
namespace {
    class BindPipelineVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(BindPipelineVulkanIntegrationTest, RetainsAndBindsGraphicsPipeline) {
        vsl::command::BindPipeline binding(*pipeline);
        EXPECT_EQ(binding.pipeline.data_, pipeline->data_);
        ExpectTriangleColor(render([&](auto& stream) { stream << binding << vsl::command::Draw{3}; }));
    }
} // namespace

#include "support/compute_fixture.hpp"
#include <atomic>
namespace {
    class BindComputePipelineVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        VkDebugUtilsMessengerEXT _abort_messenger = VK_NULL_HANDLE;
        std::atomic<unsigned> _aborted_errors{0};
        bool validate_vulkan() const override { return true; }
        static VKAPI_ATTR VkBool32 VKAPI_CALL AbortInvalidCall(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT*, void* user) {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                ++static_cast<BindComputePipelineVulkanIntegrationTest*>(user)->_aborted_errors;
                return VK_TRUE;
            }
            return VK_FALSE;
        }

    protected:
        void SetUp() override {
            ComputeVulkanIntegrationTest::SetUp();
            if (HasFatalFailure() || IsSkipped())
                return;
            // Test-only layer abort prevents an invalid bind from reaching the driver before the fix.
            VkDebugUtilsMessengerCreateInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            info.pfnUserCallback = AbortInvalidCall;
            info.pUserData = this;
            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance->instance, "vkCreateDebugUtilsMessengerEXT"));
            ASSERT_NE(create, nullptr);
            ASSERT_EQ(create(instance->instance, &info, nullptr, &_abort_messenger), VK_SUCCESS);
        }
        void TearDown() override {
            if (_abort_messenger) {
                auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance->instance, "vkDestroyDebugUtilsMessengerEXT"));
                destroy(instance->instance, _abort_messenger, nullptr);
            }
            ComputeVulkanIntegrationTest::TearDown();
        }
        void check_pipeline_holder(bool shared_command) {
            auto manager = make_manager();
            const std::uint32_t base = shared_command ? 197 : 173;
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                vsl::DefaultPhaseStream stream(manager);
                if (shared_command) {
                    auto command = std::make_shared<vsl::command::BindPipeline>(*pipeline);
                    stream << std::static_pointer_cast<vsl::command::Command>(command);
                } else {
                    stream << vsl::command::BindPipeline(*pipeline);
                }
                if (!stream.pipeline) {
                    phase.cancel();
                    FAIL() << "BindPipeline did not expose its pipeline to the stream";
                }
                ASSERT_EQ(stream.pipeline->data_, pipeline->data_);
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                       << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                       << vsl::command::Dispatch(COUNT, 1, 1);
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto commands = manager.get_buffer();
                commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device.data_->device.waitIdle();
            auto mapped = output->data(COUNT * sizeof(std::uint32_t));
            std::array<std::uint32_t, COUNT> actual{};
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            ExpectSequence(actual, base);
            EXPECT_EQ(_aborted_errors, 0u);
        }
        void check_bind(bool erase_type) {
            vsl::PipelineAccessor accessor = *pipeline;
            auto binding = erase_type ? vsl::command::BindPipeline(accessor) : vsl::command::BindPipeline(*pipeline);
            EXPECT_EQ(binding.pipeline.data_, pipeline->data_);
            auto manager = make_manager();
            const std::uint32_t base = erase_type ? 137 : 83;
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                auto stream = phase << binding;
                if (_aborted_errors != 0) {
                    phase.cancel();
                    FAIL() << "BindPipeline generated a validation error; the layer aborted the invalid driver call";
                }
                // Bind resources explicitly to isolate this test from stream PipelineHolder propagation.
                vsl::command::BindGraphicResource descriptors(resource, vsl::graphic_resource::BindingDestination::COMPUTE, accessor);
                descriptors.invoke(manager.get_pool(), manager.get_buffer(), manager);
                stream << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                       << vsl::command::Dispatch(COUNT, 1, 1);
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto commands = manager.get_buffer();
                commands.data_->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device.data_->device.waitIdle();
            auto mapped = output->data(COUNT * sizeof(std::uint32_t));
            std::array<std::uint32_t, COUNT> actual{};
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            ExpectSequence(actual, base);
            EXPECT_EQ(_aborted_errors, 0u);
        }
    };
    TEST_F(BindComputePipelineVulkanIntegrationTest, TypedComputePipelineBindsAtTheComputeBindPoint) {
        check_bind(false);
    }
    TEST_F(BindComputePipelineVulkanIntegrationTest, ErasedAccessorKeepsTheComputeBindPoint) {
        check_bind(true);
    }
    TEST_F(BindComputePipelineVulkanIntegrationTest, ValueCommandExposesThePipelineForImplicitDescriptorBinding) {
        check_pipeline_holder(false);
    }
    TEST_F(BindComputePipelineVulkanIntegrationTest, SharedCommandExposesThePipelineForImplicitDescriptorBinding) {
        check_pipeline_holder(true);
    }
    TEST_F(BindPipelineVulkanIntegrationTest, ErasedAccessorKeepsTheGraphicsBindPoint) {
        vsl::PipelineAccessor accessor = *pipeline;
        vsl::command::BindPipeline binding(accessor);
        ExpectTriangleColor(render([&](auto& stream) { stream << binding << vsl::command::Draw{3}; }));
    }
} // namespace
