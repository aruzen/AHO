#ifdef AHO_GPU_TESTS
#include "support/live_gpu.hpp"
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif
namespace {
    using AhoWindowVulkanIntegration = aho_test::LiveGpuTest;
    TEST_F(AhoWindowVulkanIntegration, ExistingWindowSupportsSizeTitleAndClose) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        EXPECT_TRUE(window.set_title("AHO changed title"));
        auto* native = static_cast<GLFWwindow*>(window._data->window_handle);
        EXPECT_STREQ(glfwGetWindowTitle(native), "AHO changed title");
        window.maximize();
        glfwPollEvents();
        EXPECT_EQ(glfwGetWindowAttrib(native, GLFW_MAXIMIZED), GLFW_TRUE);
        window.restore();
        glfwPollEvents();
        EXPECT_EQ(glfwGetWindowAttrib(native, GLFW_MAXIMIZED), GLFW_FALSE);
        EXPECT_TRUE(window.close());
        EXPECT_EQ(window._data2, nullptr);
        EXPECT_EQ(window._data->window_handle, nullptr);
    }
    TEST_F(AhoWindowVulkanIntegration, MonitorSizeRequiresAvailablePrimaryDisplayMode) {
        auto* monitor = glfwGetPrimaryMonitor();
        if (!monitor) {
            EXPECT_THROW(aho::window::GetMonitorSize(), vsl::exceptions::RuntimeException);
            return;
        }
#ifdef __APPLE__
        const auto display = glfwGetCocoaMonitor(monitor);
        auto mode = CGDisplayCopyDisplayMode(display);
        if (!mode) {
            EXPECT_THROW(aho::window::GetMonitorSize(), vsl::exceptions::RuntimeException);
            return;
        }
        CGDisplayModeRelease(mode);
#endif
        const auto* mode_info = glfwGetVideoMode(monitor);
        ASSERT_NE(mode_info, nullptr) << "GLFW failed to query a video mode exposed by the OS.";
        const auto size = aho::window::GetMonitorSize();
        EXPECT_EQ(size.value.x.value, mode_info->width);
        EXPECT_EQ(size.value.y.value, mode_info->height);
        EXPECT_GT(size.value.x.value, 0);
        EXPECT_GT(size.value.y.value, 0);
    }
    TEST_F(AhoWindowVulkanIntegration, ResizingRecreatesFrameResourcesAndCallsHook) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int callbacks = 0;
        auto hook = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* received) {
            EXPECT_EQ(received, &window);
            ++callbacks;
        });
        EXPECT_TRUE(window.resize(480, 320));
        EXPECT_EQ(callbacks, 1);
        EXPECT_EQ(window.window_size().value.x.value, 480);
        EXPECT_EQ(window.window_size().value.y.value, 320);
        EXPECT_TRUE(window.resize(aho::d2::VectorI(400, 400)));
        EXPECT_EQ(callbacks, 2);
        EXPECT_EQ(window.window_size().value.x.value, 400);
        EXPECT_EQ(window.window_size().value.y.value, 400);
    }
    TEST_F(AhoWindowVulkanIntegration, ExplicitEngineConstructorCreatesIndependentWindow) {
        ASSERT_NO_THROW(start_engine());
        const std::string name = "AHO secondary synchronization resources";
        aho::Window second(name, 240, 160, *engine);
        EXPECT_NE(second._data, engine->boot_window->_data);
        EXPECT_EQ(second.engine._data, engine->_data);
        EXPECT_EQ(second.window_size().value.x.value, 240);
        EXPECT_EQ(second.window_size().value.y.value, 160);
        EXPECT_EQ(second.name(), name);
        auto available = engine->_data->synchro_manager.getSemaphore(name + "ImageAvailable");
        auto finished = engine->_data->synchro_manager.getSemaphore(name + "RenderFinished");
        auto in_flight = engine->_data->synchro_manager.getFence(name + "InFlight");
        EXPECT_TRUE(available.has_value());
        EXPECT_TRUE(finished.has_value());
        EXPECT_TRUE(in_flight.has_value());
        if (available)
            EXPECT_EQ(available->_data, second._data2->image_available._data);
        if (finished)
            EXPECT_EQ(finished->_data, second._data2->render_finished._data);
        if (in_flight)
            EXPECT_EQ(in_flight->_data, second._data2->in_flight._data);
        EXPECT_TRUE(second.close());
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/window.hpp>
namespace {
    TEST(AhoWindowTest, SharedDataConstructorKeepsNativeOwnershipAndEmptyDrawingData) {
        aho::engine::EngineAccessor engine;
        auto native = std::make_shared<vsl::Window::WindowData>();
        std::shared_ptr<aho::window::WindowData> drawing;
        aho::Window window(engine, native, drawing);
        EXPECT_EQ(window._data, native);
        EXPECT_EQ(window._data2, drawing);
        EXPECT_EQ(window.engine._data, engine._data);
    }
    TEST(WindowResizeHookTest, ConstructorStoresCallableAndPassesWindowPointer) {
        aho::Window* received = nullptr;
        aho::window::WindowResizeHookPlugin plugin(nullptr, [&](aho::Window* window) { received = window; });
        EXPECT_TRUE(static_cast<bool>(plugin.hook));
        plugin.hook(nullptr);
        EXPECT_EQ(received, nullptr);
    }
} // namespace
#endif

#ifndef AHO_GPU_TESTS
#include <gtest/gtest.h>
#include <AHO/window.hpp>
#include <GLFW/glfw3.h>
namespace {
    TEST(MonitorSizeTest, UninitializedGlfwThrowsInsteadOfDereferencingANullMode) {
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        glfwTerminate();
        try {
            (void)aho::window::GetMonitorSize();
            ADD_FAILURE() << "Missing primary monitor was accepted";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(std::string(error.what()).find("monitor"), std::string::npos);
        }
        glfwGetError(nullptr);
    }
#if defined(GLFW_PLATFORM_NULL)
    TEST(MonitorSizeTest, InitializedNullPlatformReturnsItsVirtualMonitorSize) {
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        struct RestorePlatform {
            ~RestorePlatform() {
                glfwTerminate();
                glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
            }
        } restore;
        glfwTerminate();
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
        ASSERT_TRUE(glfwInit());
        ASSERT_NE(glfwGetPrimaryMonitor(), nullptr);
        const auto size = aho::window::GetMonitorSize();
        EXPECT_EQ(size.value.x.value, 1920);
        EXPECT_EQ(size.value.y.value, 1080);
        EXPECT_EQ(glfwGetError(nullptr), GLFW_NO_ERROR);
    }
#endif
} // namespace

#endif // AHO_GPU_TESTS
