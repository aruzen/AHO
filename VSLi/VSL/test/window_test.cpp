#include "support/window_fixture.hpp"
namespace {
using WindowIntegrationTest = vsl_test::WindowIntegrationTest;
struct TestPlugin : vsl::PureWindow::Plugin {
    int value;
    TestPlugin(vsl::PureWindow *, int value) : value(value) {}
};
TEST_F(WindowIntegrationTest, NamesSharedConstructorsComparisonsResizeAndClose) {
    EXPECT_EQ(window->name(), "VSL hidden integration test");
    EXPECT_TRUE(static_cast<bool>(*window));
    vsl::PureWindow shared(window->_data);
    EXPECT_TRUE(shared.operator==(*window));
    EXPECT_TRUE(shared == window->_data.get());
    EXPECT_TRUE(window->resize(96, 72));
    int width = 0, height = 0;
    glfwGetWindowSize(static_cast<GLFWwindow *>(window->_data->window_handle), &width, &height);
    EXPECT_EQ(width, 96);
    EXPECT_EQ(height, 72);
    EXPECT_TRUE(vsl::PureWindow::NoCheckUpdate());
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_TRUE(window->close());
    EXPECT_FALSE(static_cast<bool>(*window));
    EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
}
TEST_F(WindowIntegrationTest, AddsAndRemovesPluginByIdentity) {
    auto plugin = window->add_plugin<TestPlugin>(17);
    EXPECT_EQ(plugin->value, 17);
    EXPECT_EQ(window->_data->plugins.at(plugin->id), plugin);
    EXPECT_TRUE(window->remove_plugin(plugin));
    EXPECT_FALSE(window->remove_plugin(plugin));
}
TEST_F(WindowIntegrationTest, WindowConstructorsInstallQuietCloseOnce) {
    vsl::Window shared(window->_data);
    EXPECT_EQ(shared._data, window->_data);
    ASSERT_TRUE(shared._data->plugins.contains(vsl::window_plugin::QuietClose::QUIET_CLOSE_USED_ID));
    vsl::Window another(shared._data);
    EXPECT_EQ(another._data->update.size(), 1u);
    vsl::Window separate("another hidden window", 32, 24);
    EXPECT_TRUE(static_cast<bool>(separate));
    EXPECT_EQ(separate.name(), "another hidden window");
    EXPECT_TRUE(separate.close());
}
} // namespace
