#pragma once
#include "vulkan_fixture.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <optional>
#include <VSL/lifetime/glfw_lease.hpp>
namespace vsl_test {
    class WindowIntegrationTest : public VulkanIntegrationTest {
    protected:
        vsl::lifetime::GLFWLease glfw_lease;
        std::optional<vsl::PureWindow> window;
        std::shared_ptr<vsl::Surface> surface;
        void SetUp() override;
        void make_surface() {
            surface = window->add_plugin<vsl::Surface>(vsl::VulkanAccessor{instance});
            ASSERT_TRUE(surface->data_->surface) << "GLFW surface creation failed";
        }
        void TearDown() override;
    };

    inline void WindowIntegrationTest::SetUp() {
        VulkanIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        try {
            glfw_lease = vsl::lifetime::GLFWLease::Acquire();
        } catch (const vsl::exceptions::RuntimeException& error) {
            const std::string reason = error.what();
            if (reason.find(std::to_string(GLFW_PLATFORM_UNAVAILABLE)) != std::string::npos || reason.find("DISPLAY") != std::string::npos ||
                reason.find("display connection") != std::string::npos)
                GTEST_SKIP() << "Window/display unavailable: " << reason;
            FAIL() << "GLFW initialization failed: " << reason;
        }
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef _WIN32
        // Hidden test windows do not need the caption's minimum width.
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif
        window.emplace("VSL hidden integration test", 64, 48);
        ASSERT_TRUE(static_cast<bool>(*window)) << "Hidden GLFW window creation failed";
    }

    inline void WindowIntegrationTest::TearDown() {
        if (device.data_)
            device.data_->device.waitIdle();
        if (window)
            window->data_->plugins.clear();
        surface.reset();
        if (window && static_cast<bool>(*window))
            window->close();
        window.reset();
        glfw_lease = {};
        vsl::lifetime::GLFWLease::Drain();
        VulkanIntegrationTest::TearDown();
    }
} // namespace vsl_test
