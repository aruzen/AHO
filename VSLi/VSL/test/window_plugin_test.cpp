#include "support/window_fixture.hpp"

namespace {
using WindowPluginWindowIntegrationTest = vsl_test::WindowIntegrationTest;
TEST_F(WindowPluginWindowIntegrationTest, QuietCloseHonorsNativeShouldCloseFlag) {
    vsl::window_plugin::QuietClose plugin(&*window);
    EXPECT_EQ(plugin.id, vsl::window_plugin::QuietClose::QUIET_CLOSE_USED_ID);
    EXPECT_TRUE(plugin.onUpdate(*window));
    EXPECT_TRUE(static_cast<bool>(*window));
    glfwSetWindowShouldClose(static_cast<GLFWwindow *>(window->_data->window_handle), GLFW_TRUE);
    EXPECT_TRUE(plugin.onUpdate(*window));
    EXPECT_FALSE(static_cast<bool>(*window));
}
TEST_F(WindowPluginWindowIntegrationTest, CloseHookInvokesCallbackOnlyWhenRequested) {
    int called = 0;
    vsl::window_plugin::HookShouldClose hook(window->_data.get(), [&](auto copy) {
        EXPECT_EQ(copy._data, window->_data);
        ++called;
    });
    EXPECT_TRUE(hook.onUpdate(*window));
    EXPECT_EQ(called, 0);
    glfwSetWindowShouldClose(static_cast<GLFWwindow *>(window->_data->window_handle), GLFW_TRUE);
    EXPECT_TRUE(hook.onUpdate(*window));
    EXPECT_EQ(called, 1);
}
TEST_F(WindowPluginWindowIntegrationTest, ResizeHookRoutesFramebufferDimensionsToCallback) {
    int called = 0, width = 0, height = 0;
    auto plugin = window->add_plugin<vsl::window_plugin::HookResize>([&](auto copy, int w, int h) {
        EXPECT_EQ(copy._data, window->_data);
        ++called;
        width = w;
        height = h;
    });
    EXPECT_EQ(plugin->id, vsl::window_plugin::HookResize::HOOK_RESIZE_USED_ID);
    auto handle = static_cast<GLFWwindow *>(window->_data->window_handle);
    const auto callback = glfwSetFramebufferSizeCallback(handle, nullptr);
    ASSERT_NE(callback, nullptr);
    glfwSetFramebufferSizeCallback(handle, callback);
    callback(handle, 128, 96);
    EXPECT_EQ(called, 1);
    EXPECT_EQ(width, 128);
    EXPECT_EQ(height, 96);
}
TEST_F(WindowPluginWindowIntegrationTest, CloseHookCanBeAddedWithPublicPluginHelper) {
    int called = 0;
    auto hook = window->add_plugin<vsl::window_plugin::HookShouldClose>([&](vsl::PureWindow copy) {
        EXPECT_EQ(copy._data, window->_data);
        ++called;
    });
    EXPECT_EQ(window->_data->plugins.at(hook->id), hook);
    ASSERT_EQ(window->_data->update.size(), 1u);
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(called, 0);
    glfwSetWindowShouldClose(static_cast<GLFWwindow *>(window->_data->window_handle), GLFW_TRUE);
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(called, 1);
}

} // namespace

TEST(WindowPluginTest, KeepsWindowDataAndNullPointerConstructionAvailable) {
    using Hook = vsl::window_plugin::HookShouldClose;
    using Callback = std::function<void(vsl::PureWindow)>;
    static_assert(std::is_constructible_v<Hook, vsl::PureWindow *, Callback>);
    static_assert(std::is_constructible_v<Hook, vsl::PureWindow::WindowData *, Callback>);
    static_assert(std::is_constructible_v<Hook, std::nullptr_t, Callback>);
    static_assert(!std::is_constructible_v<Hook, int *, Callback>);
    int called = 0;
    Hook hook(nullptr, [&](vsl::PureWindow) { ++called; });
    EXPECT_EQ(called, 0);
    hook.callback(vsl::PureWindow(std::make_shared<vsl::PureWindow::WindowData>()));
    EXPECT_EQ(called, 1);
}
