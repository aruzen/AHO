#include "support/window_fixture.hpp"
#include "support/compile_probe.hpp"
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
TEST(WindowPluginTest, DISABLED_CloseHookCanBeAddedWithPublicPluginHelper) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/window_plugin.hpp>
void probe(vsl::PureWindow& window) {
    window.add_plugin<vsl::window_plugin::HookShouldClose>([](vsl::PureWindow){});
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
} // namespace
