#include "../src/private/VSL/lifetime/window_access.hpp"
namespace {
    using VSL_NAMESPACE::lifetime::detail::WindowAccess;
}
#include "support/window_fixture.hpp"

namespace {
    using WindowPluginWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(WindowPluginWindowIntegrationTest, QuietCloseHonorsNativeShouldCloseFlag) {
        vsl::window_plugin::QuietClose plugin(&*window);
        EXPECT_EQ(plugin.requested_id(), vsl::window_plugin::QuietClose::QUIET_CLOSE_USED_ID);
        EXPECT_TRUE(plugin.on_update(*window));
        EXPECT_TRUE(static_cast<bool>(*window));
        glfwSetWindowShouldClose(static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle), GLFW_TRUE);
        EXPECT_TRUE(plugin.on_update(*window));
        EXPECT_FALSE(static_cast<bool>(*window));
    }
    TEST_F(WindowPluginWindowIntegrationTest, CloseHookInvokesCallbackOnlyWhenRequested) {
        int called = 0;
        vsl::window_plugin::HookShouldClose hook(&*window, [&](auto copy) {
            EXPECT_EQ(WindowAccess::State(copy), WindowAccess::State(*(window)));
            ++called;
        });
        EXPECT_TRUE(hook.on_update(*window));
        EXPECT_EQ(called, 0);
        glfwSetWindowShouldClose(static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle), GLFW_TRUE);
        EXPECT_TRUE(hook.on_update(*window));
        EXPECT_EQ(called, 1);
    }
    TEST_F(WindowPluginWindowIntegrationTest, ResizeHookRoutesFramebufferDimensionsToCallback) {
        int called = 0, width = 0, height = 0;
        auto plugin = window->add_plugin<vsl::window_plugin::HookResize>([&](auto copy, int w, int h) {
            EXPECT_EQ(WindowAccess::State(copy), WindowAccess::State(*(window)));
            ++called;
            width = w;
            height = h;
        });
        EXPECT_EQ(plugin->requested_id(), vsl::window_plugin::HookResize::HOOK_RESIZE_USED_ID);
        auto handle = static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle);
        const auto callback = glfwSetFramebufferSizeCallback(handle, nullptr);
        ASSERT_NE(callback, nullptr);
        glfwSetFramebufferSizeCallback(handle, callback);
        callback(handle, 128, 96);
        EXPECT_EQ(called, 1);
        EXPECT_EQ(width, 128);
        EXPECT_EQ(height, 96);
    }
    TEST_F(WindowPluginWindowIntegrationTest, ResizeHookCanCreateWindowsWithoutInvalidatingDispatch) {
        vsl::PureWindow other("resize hook neighbor", 32, 24);
        std::vector<vsl::PureWindow> created;
        const auto previous_capacity = WindowAccess::Registry().capacity();
        const auto extra = previous_capacity - WindowAccess::Registry().size() + 1;
        bool entered = false;
        auto hook = window->add_plugin<vsl::window_plugin::HookResize>([&](auto, int, int) {
            if (entered)
                return;
            entered = true;
            for (std::size_t i = 0; i < extra; ++i)
                created.emplace_back("created by resize hook", 32, 24);
        });
        auto handle = static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle);
        const auto callback = glfwSetFramebufferSizeCallback(handle, nullptr);
        EXPECT_NE(callback, nullptr);
        glfwSetFramebufferSizeCallback(handle, callback);
        if (callback)
            callback(handle, 128, 96);
        EXPECT_GT(WindowAccess::Registry().capacity(), previous_capacity);
        EXPECT_EQ(created.size(), extra);
        EXPECT_TRUE(static_cast<bool>(other));
        for (auto& added : created)
            added.close();
        other.close();
    }
    TEST_F(WindowPluginWindowIntegrationTest, CloseHookCanBeAddedWithPublicPluginHelper) {
        int called = 0;
        auto hook = window->add_plugin<vsl::window_plugin::HookShouldClose>([&](vsl::PureWindow copy) {
            EXPECT_EQ(WindowAccess::State(copy), WindowAccess::State(*(window)));
            ++called;
        });
        EXPECT_EQ(WindowAccess::State(*(window))->plugins.at(*window->plugin_id(hook)), hook);
        ASSERT_EQ(WindowAccess::State(*(window))->update.size(), 1u);
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(called, 0);
        glfwSetWindowShouldClose(static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle), GLFW_TRUE);
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(called, 1);
    }

} // namespace

namespace {
    TEST_F(WindowPluginWindowIntegrationTest, CloseCallbackCanClearItselfBeforeFinishing) {
        int calls = 0;
        std::shared_ptr<vsl::window_plugin::HookShouldClose> hook;
        hook = window->add_plugin<vsl::window_plugin::HookShouldClose>([&](vsl::PureWindow current) {
            hook->callback = {};
            EXPECT_EQ(current, *window);
            ++calls;
        });
        window->set_should_close(true);
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(calls, 1);
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(calls, 1);
    }
    TEST_F(WindowPluginWindowIntegrationTest, ResizeCallbackCanReplaceItsOwnRegistrationAndContinue) {
        int calls = 0;
        std::shared_ptr<vsl::window_plugin::HookResize> replacement;
        auto hook = window->add_plugin<vsl::window_plugin::HookResize>([&](vsl::PureWindow current, int, int) {
            replacement = current.add_plugin<vsl::window_plugin::HookResize>([](auto, int, int) {});
            ++calls;
        });
        const auto os = window->window_owner();
        auto* native = static_cast<GLFWwindow*>(os.native_handle());
        const auto callback = glfwSetFramebufferSizeCallback(native, nullptr);
        glfwSetFramebufferSizeCallback(native, callback);
        ASSERT_NE(callback, nullptr);
        callback(native, 128, 96);
        EXPECT_EQ(calls, 1);
        EXPECT_FALSE(window->plugin_id(hook));
        EXPECT_EQ(window->plugin(vsl::window_plugin::HookResize::HOOK_RESIZE_USED_ID), replacement);
    }
} // namespace

TEST(WindowPluginTest, KeepsFacadeAndNullPointerConstructionAvailable) {
    using Hook = vsl::window_plugin::HookShouldClose;
    using Callback = std::function<void(vsl::PureWindow)>;
    static_assert(std::is_constructible_v<Hook, vsl::PureWindow*, Callback>);
    static_assert(!std::is_constructible_v<Hook, vsl::PureWindow::WindowData*, Callback>);
    static_assert(std::is_constructible_v<Hook, std::nullptr_t, Callback>);
    static_assert(!std::is_constructible_v<Hook, int*, Callback>);
    int called = 0;
    Hook hook(nullptr, [&](vsl::PureWindow) { ++called; });
    EXPECT_EQ(called, 0);
    hook.callback(vsl::PureWindow(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>())));
    EXPECT_EQ(called, 1);
}
