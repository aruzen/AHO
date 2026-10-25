#pragma once
#include <gtest/gtest.h>
#include <AHO/engine/standard_engine.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <memory>
#include <mutex>
#include <string_view>

namespace aho_test {
    class LiveGPUTest : public testing::Test {
        vsl::LoggerT _previous_logger = nullptr;
        inline static std::vector<std::string> VALIDATION_ERRORS;
        inline static std::mutex VALIDATION_MUTEX;
        static void CaptureLog(const char* text);

    protected:
        std::unique_ptr<aho::StandardEngine> engine;
        void SetUp() override;
        void start_engine(bool boot_on = true) {
            engine = std::make_unique<aho::StandardEngine>("AHO integration test", boot_on);
        }
        void TearDown() override;
    };

    inline void LiveGPUTest::CaptureLog(const char* text) {
        const std::string_view message(text);
        if (message.find("Validation Error") != std::string_view::npos || message.find("VUID-") != std::string_view::npos) {
            std::lock_guard lock(VALIDATION_MUTEX);
            VALIDATION_ERRORS.emplace_back(text);
        }
    }

    inline void LiveGPUTest::SetUp() {
        _previous_logger = vsl::LOGGER;
        {
            std::lock_guard lock(VALIDATION_MUTEX);
            VALIDATION_ERRORS.clear();
        }
        vsl::LOGGER = CaptureLog;
        if (glfwInit() != GLFW_TRUE) {
            const char* reason = nullptr;
            glfwGetError(&reason);
            GTEST_SKIP() << "GLFW display is unavailable: " << (reason ? reason : "unknown initialization error");
        }
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &app;
#ifdef __APPLE__
        const char* portability = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = &portability;
#endif
        VkInstance instance = VK_NULL_HANDLE;
        const auto result = vkCreateInstance(&info, nullptr, &instance);
        if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
            GTEST_SKIP() << "No usable Vulkan driver / Metal device in this execution environment.";
        ASSERT_EQ(result, VK_SUCCESS) << "Vulkan is installed but instance initialization failed.";
        std::uint32_t count = 0;
        const auto enumerated = vkEnumeratePhysicalDevices(instance, &count, nullptr);
        vkDestroyInstance(instance, nullptr);
        ASSERT_EQ(enumerated, VK_SUCCESS);
        if (count == 0)
            GTEST_SKIP() << "Vulkan exposes no physical devices.";
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    inline void LiveGPUTest::TearDown() {
        if (engine && engine->data_ && engine->data_->logical_device.data_)
            engine->data_->logical_device.data_->device.waitIdle();
        if (engine && engine->boot_window && engine->boot_window->data_ && engine->boot_window->data_->window_handle)
            engine->boot_window->close();
        engine.reset();
        // The library closes its GLFW windows explicitly; never leave a failed case's window open.
        const auto windows = vsl::PureWindow::WINDOWS;
        for (const auto& window : windows)
            if (window->window_handle)
                vsl::PureWindow(window).close();
        glfwTerminate();
        vsl::LOGGER = _previous_logger;
        std::vector<std::string> errors;
        {
            std::lock_guard lock(VALIDATION_MUTEX);
            errors.swap(VALIDATION_ERRORS);
        }
        for (const auto& error : errors)
            ADD_FAILURE() << error;
    }
} // namespace aho_test
