#pragma once
#include "vulkan_fixture.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <optional>
namespace vsl_test {
    class WindowIntegrationTest : public VulkanIntegrationTest {
    protected:
        std::optional<vsl::PureWindow> window;
        std::shared_ptr<vsl::Surface> surface;
        void SetUp() override {
            VulkanIntegrationTest::SetUp();
            if (HasFatalFailure() || IsSkipped())
                return;
            if (!glfwInit()) {
                const char *description = nullptr;
                const int error = glfwGetError(&description);
                const std::string reason = description ? description : "unknown GLFW error";
                if (error == GLFW_PLATFORM_UNAVAILABLE || reason.find("DISPLAY") != std::string::npos ||
                    reason.find("display connection") != std::string::npos)
                    GTEST_SKIP() << "Window/display unavailable: " << reason;
                FAIL() << "GLFW initialization failed: " << error << " " << reason;
            }
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            window.emplace("VSL hidden integration test", 64, 48);
            ASSERT_TRUE(static_cast<bool>(*window)) << "Hidden GLFW window creation failed";
        }
        void make_surface() {
            surface = window->add_plugin<vsl::Surface>(vsl::VulkanAccessor{instance});
            ASSERT_TRUE(surface->_data->surface) << "GLFW surface creation failed";
        }
        void TearDown() override {
            if (device._data)
                device._data->device.waitIdle();
            if (window)
                window->_data->plugins.clear();
            surface.reset();
            if (window && static_cast<bool>(*window))
                window->close();
            window.reset();
            glfwTerminate();
            VulkanIntegrationTest::TearDown();
        }
    };
} // namespace vsl_test
