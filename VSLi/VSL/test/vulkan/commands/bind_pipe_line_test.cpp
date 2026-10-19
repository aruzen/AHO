#include "support/render_fixture.hpp"
namespace {
    class BindPipelineVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(BindPipelineVulkanIntegrationTest, RetainsAndBindsGraphicsPipeline) {
        vsl::command::BindPipeline binding(*pipeline);
        EXPECT_EQ(binding.pipeline._data, pipeline->_data);
        expect_triangle_color(render([&](auto& stream) { stream << binding << vsl::command::Draw{3}; }));
    }
} // namespace

#include "support/compute_fixture.hpp"
#include <atomic>
namespace {
    class BindComputePipelineVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        VkDebugUtilsMessengerEXT abort_messenger_ = VK_NULL_HANDLE;
        std::atomic<unsigned> aborted_errors_{0};
        bool validate_vulkan() const override { return true; }
        static VKAPI_ATTR VkBool32 VKAPI_CALL abort_invalid_call(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT*, void* user) {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                ++static_cast<BindComputePipelineVulkanIntegrationTest*>(user)->aborted_errors_;
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
            info.pfnUserCallback = abort_invalid_call;
            info.pUserData = this;
            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance->instance, "vkCreateDebugUtilsMessengerEXT"));
            ASSERT_NE(create, nullptr);
            ASSERT_EQ(create(instance->instance, &info, nullptr, &abort_messenger_), VK_SUCCESS);
        }
        void TearDown() override {
            if (abort_messenger_) {
                auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance->instance, "vkDestroyDebugUtilsMessengerEXT"));
                destroy(instance->instance, abort_messenger_, nullptr);
            }
            ComputeVulkanIntegrationTest::TearDown();
        }
        void check_bind(bool erase_type) {
            vsl::PipelineAccessor accessor = *pipeline;
            auto binding = erase_type ? vsl::command::BindPipeline(accessor) : vsl::command::BindPipeline(*pipeline);
            EXPECT_EQ(binding.pipeline._data, pipeline->_data);
            auto manager = make_manager();
            const std::uint32_t base = erase_type ? 137 : 83;
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                auto stream = phase << binding;
                if (aborted_errors_ != 0) {
                    phase.cancel();
                    FAIL() << "BindPipeline generated a validation error; the layer aborted the invalid driver call";
                }
                // Bind resources explicitly to isolate this test from stream PipelineHolder propagation.
                vsl::command::BindGraphicResource descriptors(resource, vsl::graphic_resource::BindingDestination::Compute, accessor);
                descriptors.invoke(manager.getPool(), manager.getBuffer(), manager);
                stream << vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, base)
                       << vsl::command::Dispatch(count, 1, 1);
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto commands = manager.getBuffer();
                commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device._data->device.waitIdle();
            auto mapped = output->data(count * sizeof(std::uint32_t));
            std::array<std::uint32_t, count> actual{};
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            expect_sequence(actual, base);
            EXPECT_EQ(aborted_errors_, 0u);
        }
    };
    TEST_F(BindComputePipelineVulkanIntegrationTest, TypedComputePipelineBindsAtTheComputeBindPoint) {
        check_bind(false);
    }
    TEST_F(BindComputePipelineVulkanIntegrationTest, ErasedAccessorKeepsTheComputeBindPoint) {
        check_bind(true);
    }
    TEST_F(BindPipelineVulkanIntegrationTest, ErasedAccessorKeepsTheGraphicsBindPoint) {
        vsl::PipelineAccessor accessor = *pipeline;
        vsl::command::BindPipeline binding(accessor);
        expect_triangle_color(render([&](auto& stream) { stream << binding << vsl::command::Draw{3}; }));
    }
} // namespace
