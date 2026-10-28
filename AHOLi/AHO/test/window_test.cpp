#ifdef AHO_GPU_TESTS
#include "support/live_gpu.hpp"
#include <AHO/drawer/draw_phase.hpp>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif
namespace {
    using AHOWindowVulkanIntegration = aho_test::LiveGPUTest;

    struct FixedIdResizeHook : aho::window::WindowResizeHookPlugin {
        FixedIdResizeHook(vsl::PureWindow* window, unsigned char id, std::function<void(aho::Window*)> callback)
            : WindowResizeHookPlugin(window, std::move(callback)) { this->id = id; }
    };

    bool expect_recreation_guard(aho::Window& window) {
        // ガードが欠けても、VSLのusage検証で止めて記録中のswapchainを破棄しない。
        const auto unsupported = static_cast<vsl::ImageType>(std::uint32_t{1} << 31);
        try {
            window.recreate_swapchain(unsupported);
            ADD_FAILURE() << "Recording or reentrant recreation was accepted";
        } catch (const std::invalid_argument& error) {
            ADD_FAILURE() << "Recreation reached usage validation before its guard: " << error.what();
        } catch (const std::logic_error&) {
            return true;
        }
        return false;
    }

    TEST_F(AHOWindowVulkanIntegration, RecreationNotifiesAfterReplacingResourcesWithoutResizingTheNativeWindow) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto previous_swapchain = window.data2->swapchain.data_;
        const auto previous_frames = window.data2->frame_buffer.data_;
        const auto previous_pass = window.data2->render_pass.data_;
        const auto previous_usage = window.data2->swapchain.images.front().get_usage();
        const auto size = window.window_size();
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++calls;
            EXPECT_NE(current->data2->swapchain.data_, previous_swapchain);
            EXPECT_NE(current->data2->frame_buffer.data_, previous_frames);
            EXPECT_EQ(current->data2->render_pass.data_, previous_pass);
            EXPECT_EQ(current->data2->swapchain.images.front().get_usage(), previous_usage);
            EXPECT_EQ(current->data2->render_finished.size(), current->data2->swapchain.get_swap_image_size());
        });
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(window.window_size().value.x.value, size.value.x.value);
        EXPECT_EQ(window.window_size().value.y.value, size.value.y.value);
    }

    TEST_F(AHOWindowVulkanIntegration, RecordingGuardPrecedesNativeResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto size = window.window_size();
        const auto previous = window.data2->swapchain.data_;
        aho::DrawPhase phase(*engine);
        EXPECT_THROW(window.resize(480, 320), std::logic_error);
        EXPECT_EQ(window.window_size().value.x.value, size.value.x.value);
        EXPECT_EQ(window.window_size().value.y.value, size.value.y.value);
        EXPECT_EQ(window.data2->swapchain.data_, previous);
        expect_recreation_guard(window);
    }

    TEST_F(AHOWindowVulkanIntegration, RecordingGuardFindsTheOuterPhaseOfAnotherEngine) {
        ASSERT_NO_THROW(start_engine());
        auto close_engine = [](aho::StandardEngine* other) {
            other->data()->command_manager.wait_idle();
            other->boot_window->close();
            delete other;
        };
        std::unique_ptr<aho::StandardEngine, decltype(close_engine)> other(new aho::StandardEngine("nested recreation"), close_engine);
        auto& window = *engine->boot_window;
        const auto previous = window.data2->swapchain.data_;
        aho::DrawPhase outer(*engine);
        {
            aho::DrawPhase inner(*other);
            expect_recreation_guard(window);
            EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &inner);
            EXPECT_EQ(outer.state, vsl::ComputePhase::State::WAIT_SUBMIT);
            EXPECT_EQ(window.data2->swapchain.data_, previous);
        }
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &outer);
    }

    TEST_F(AHOWindowVulkanIntegration, NotificationRejectsRecreationThroughACopiedWindow) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++calls;
            auto copy = *current;
            if (!expect_recreation_guard(copy))
                return;
            const auto size = copy.window_size();
            EXPECT_THROW(copy.resize(480, 320), std::logic_error);
            EXPECT_EQ(copy.window_size().value.x.value, size.value.x.value);
            EXPECT_EQ(copy.window_size().value.y.value, size.value.y.value);
        });
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(calls, 1);
    }

    TEST_F(AHOWindowVulkanIntegration, ThrowingNotificationAllowsAFollowingRecreation) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) {
            if (++calls == 1)
                throw std::runtime_error("resize listener failed");
        });
        EXPECT_THROW(window.recreate_swapchain(), std::runtime_error);
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(calls, 2);
    }

    TEST_F(AHOWindowVulkanIntegration, ClosedWindowRejectsRecreationAndResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        EXPECT_TRUE(window.close());
        EXPECT_THROW(window.recreate_swapchain(), std::logic_error);
        EXPECT_THROW(window.resize(480, 320), std::logic_error);
    }

    TEST_F(AHOWindowVulkanIntegration, ResizeHookCanRemoveItselfAndIsReleasedBeforeTheNextHook) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int self_calls = 0, follower_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> self;
        std::weak_ptr<aho::window::WindowResizeHookPlugin> weak;
        self = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++self_calls;
            EXPECT_TRUE(current->remove_plugin(self));
            self.reset();
            EXPECT_FALSE(weak.expired());
        });
        weak = self;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) {
            ++follower_calls;
            EXPECT_TRUE(weak.expired());
        });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(self_calls, 1);
        EXPECT_EQ(follower_calls, 1);
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(self_calls, 1);
        EXPECT_EQ(follower_calls, 2);
    }
    TEST_F(AHOWindowVulkanIntegration, HooksAddedDuringResizeWaitUntilTheNextNotification) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int first_calls = 0, added_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> added;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++first_calls;
            if (!added)
                added = current->add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++added_calls; });
        });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(first_calls, 1);
        EXPECT_EQ(added_calls, 0);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(first_calls, 2);
        EXPECT_EQ(added_calls, 1);
    }
    TEST_F(AHOWindowVulkanIntegration, RemovedHooksAreSkippedEvenIfTheCallerStillOwnsThem) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int removed_calls = 0, remaining_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> removed;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            if (removed)
                current->remove_plugin(removed);
        });
        removed = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++removed_calls; });
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++remaining_calls; });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(removed_calls, 0);
        EXPECT_EQ(remaining_calls, 1);
        EXPECT_TRUE(static_cast<bool>(removed));
    }
    TEST_F(AHOWindowVulkanIntegration, ClosingTheWindowDuringAResizeStopsItsRemainingHooks) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int stale_calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([](aho::Window* current) { EXPECT_TRUE(current->close()); });
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++stale_calls; });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(stale_calls, 0);
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_FALSE(window.data2);
    }
    TEST_F(AHOWindowVulkanIntegration, ClearingAllPluginsDuringAResizeKeepsTheRunningHookAlive) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int stale_calls = 0;
        std::weak_ptr<aho::window::WindowResizeHookPlugin> weak;
        auto current_hook = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            current->data_->plugins.clear();
            EXPECT_FALSE(weak.expired());
        });
        weak = current_hook;
        current_hook.reset();
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++stale_calls; });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_TRUE(weak.expired());
        EXPECT_EQ(stale_calls, 0);
        EXPECT_TRUE(window.data_->plugins.empty());
        EXPECT_TRUE(window.resize(400, 400));
    }
    TEST_F(AHOWindowVulkanIntegration, ReplacementAtTheSameIdWaitsUntilTheNextResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int old_calls = 0, replacement_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> old;
        std::shared_ptr<FixedIdResizeHook> replacement;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            if (!replacement)
                replacement = current->add_plugin<FixedIdResizeHook>(old->id, [&](aho::Window*) { ++replacement_calls; });
        });
        old = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++old_calls; });
        ASSERT_NE(old->id, 0u);
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(old_calls, 0);
        EXPECT_EQ(replacement_calls, 0);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(old_calls, 0);
        EXPECT_EQ(replacement_calls, 1);
    }
    TEST_F(AHOWindowVulkanIntegration, GraphicsInitializationFailureUnregistersTheNewNativeWindow) {
        ASSERT_NO_THROW(start_engine());
        aho::engine::EngineAccessor failing(std::make_shared<aho::engine::EngineData>(*engine->data()));
        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#ifdef __APPLE__
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        auto unsupported = std::make_shared<vsl::impl::VulkanImpl<false>>();
        vk::InstanceCreateInfo info{};
#ifdef __APPLE__
        info.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif
        info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        unsupported->instance = vk::createInstance(info);
        failing.data()->vulkan_instance.accessor = unsupported;
        const auto previous = vsl::PureWindow::WINDOWS;
        const auto previous_default = aho::engine::DefaultEngineRegistration::Current().data();
        EXPECT_THROW((aho::Window("cannot create graphics", 48, 32, failing)), vsl::exceptions::RuntimeException);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, previous);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), previous_default);
        EXPECT_TRUE(static_cast<bool>(*engine->boot_window));
        aho::Window retry("valid construction after failure", 48, 32, *engine);
        EXPECT_TRUE(static_cast<bool>(retry));
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), previous.size() + 1);
        EXPECT_TRUE(retry.close());
    }
    TEST_F(AHOWindowVulkanIntegration, BootedDefaultEngineCreatesAndClosesAnIndependentWindow) {
        ASSERT_NO_THROW(start_engine());
        const auto previous = vsl::PureWindow::WINDOWS.size();
        aho::Window second("default engine construction", 48, 32);
        EXPECT_EQ(second.engine.data(), engine->data());
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), previous + 1);
        EXPECT_TRUE(second.close());
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), previous);
    }
    TEST_F(AHOWindowVulkanIntegration, ExistingWindowSupportsSizeTitleAndClose) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        EXPECT_TRUE(window.set_title("AHO changed title"));
        auto* native = static_cast<GLFWwindow*>(window.data_->window_handle);
        EXPECT_STREQ(glfwGetWindowTitle(native), "AHO changed title");
        window.maximize();
        glfwPollEvents();
        EXPECT_EQ(glfwGetWindowAttrib(native, GLFW_MAXIMIZED), GLFW_TRUE);
        window.restore();
        glfwPollEvents();
        EXPECT_EQ(glfwGetWindowAttrib(native, GLFW_MAXIMIZED), GLFW_FALSE);
        EXPECT_TRUE(window.close());
        EXPECT_EQ(window.data2, nullptr);
        EXPECT_EQ(window.data_->window_handle, nullptr);
    }
    TEST_F(AHOWindowVulkanIntegration, MonitorSizeRequiresAvailablePrimaryDisplayMode) {
        auto* monitor = glfwGetPrimaryMonitor();
        if (!monitor) {
            EXPECT_THROW(aho::window::get_monitor_size(), vsl::exceptions::RuntimeException);
            return;
        }
#ifdef __APPLE__
        const auto display = glfwGetCocoaMonitor(monitor);
        auto mode = CGDisplayCopyDisplayMode(display);
        if (!mode) {
            EXPECT_THROW(aho::window::get_monitor_size(), vsl::exceptions::RuntimeException);
            return;
        }
        CGDisplayModeRelease(mode);
#endif
        const auto* mode_info = glfwGetVideoMode(monitor);
        ASSERT_NE(mode_info, nullptr) << "GLFW failed to query a video mode exposed by the OS.";
        const auto size = aho::window::get_monitor_size();
        EXPECT_EQ(size.value.x.value, mode_info->width);
        EXPECT_EQ(size.value.y.value, mode_info->height);
        EXPECT_GT(size.value.x.value, 0);
        EXPECT_GT(size.value.y.value, 0);
    }
    TEST_F(AHOWindowVulkanIntegration, ResizingRecreatesFrameResourcesAndCallsHook) {
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
    TEST_F(AHOWindowVulkanIntegration, ExplicitEngineConstructorCreatesIndependentWindow) {
        ASSERT_NO_THROW(start_engine());
        const std::string name = "AHO secondary synchronization resources";
        aho::Window second(name, 240, 160, *engine);
        EXPECT_NE(second.data_, engine->boot_window->data_);
        EXPECT_EQ(second.engine.data(), engine->data());
        EXPECT_EQ(second.window_size().value.x.value, 240);
        EXPECT_EQ(second.window_size().value.y.value, 160);
        EXPECT_EQ(second.name(), name);
        auto available = engine->data()->synchro_manager.get_semaphore(name + "ImageAvailable");
        auto finished = engine->data()->synchro_manager.get_semaphore(name + "RenderFinished");
        auto in_flight = engine->data()->synchro_manager.get_fence(name + "InFlight");
        EXPECT_TRUE(available.has_value());
        EXPECT_TRUE(finished.has_value());
        EXPECT_TRUE(in_flight.has_value());
        if (available)
            EXPECT_EQ(available->data_, second.data2->image_available.data_);
        if (finished)
            EXPECT_EQ(finished->data_, second.data2->render_finished.data_);
        if (in_flight)
            EXPECT_EQ(in_flight->data_, second.data2->in_flight.data_);
        EXPECT_TRUE(second.close());
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/window.hpp>
#include <AHO/engine/standard_engine.hpp>
namespace {
    TEST(AHOWindowConstructionTest, MissingDefaultEngineThrowsBeforeWindowRegistration) {
        ASSERT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_THROW((aho::Window("no default engine")), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
    }
    TEST(AHOWindowConstructionTest, UnbootedDefaultEngineThrowsBeforeWindowRegistration) {
        aho::StandardEngine engine("not booted", false);
        ASSERT_FALSE(engine.data());
        EXPECT_THROW((aho::Window("unbooted default engine")), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
    }
    TEST(AHOWindowConstructionTest, UnbootedExplicitEngineThrowsBeforeWindowRegistration) {
        aho::StandardEngine engine("not booted", false);
        ASSERT_FALSE(engine.data());
        EXPECT_THROW((aho::Window("unbooted explicit engine", 48, 32, engine)), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
    }
    TEST(AHOWindowTest, SharedDataConstructorKeepsNativeOwnershipAndEmptyDrawingData) {
        aho::engine::EngineAccessor engine;
        auto native = std::make_shared<vsl::Window::WindowData>();
        std::shared_ptr<aho::window::WindowData> drawing;
        aho::Window window(engine, native, drawing);
        EXPECT_EQ(window.data_, native);
        EXPECT_EQ(window.data2, drawing);
        EXPECT_EQ(window.engine.data(), engine.data());
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
            (void)aho::window::get_monitor_size();
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
        const auto size = aho::window::get_monitor_size();
        EXPECT_EQ(size.value.x.value, 1920);
        EXPECT_EQ(size.value.y.value, 1080);
        EXPECT_EQ(glfwGetError(nullptr), GLFW_NO_ERROR);
    }
#endif
} // namespace

#endif // AHO_GPU_TESTS
