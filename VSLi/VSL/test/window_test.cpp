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

namespace {
struct CountingUpdatePlugin : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
    int &calls;
    CountingUpdatePlugin(vsl::PureWindow *, int &calls) : calls(calls) {}
    bool onUpdate(vsl::PureWindow) override { ++calls; return true; }
};
TEST(WindowPluginRemovalTest, RemovesTheUpdateSubscriptionAndReleasesItsOwnership) {
    vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
    int removed_calls = 0, remaining_calls = 0;
    auto removed = window.add_plugin<CountingUpdatePlugin>(removed_calls);
    auto remaining = window.add_plugin<CountingUpdatePlugin>(remaining_calls);
    std::weak_ptr<CountingUpdatePlugin> weak = removed;
    ASSERT_EQ(window._data->update.size(), 2u);
    ASSERT_TRUE(window.remove_plugin(removed));
    EXPECT_FALSE(window.remove_plugin(removed));
    ASSERT_EQ(window._data->update.size(), 1u);
    EXPECT_EQ(window._data->update.front(), remaining);
    for (const auto &subscriber : window._data->update)
        EXPECT_TRUE(subscriber->onUpdate(window));
    EXPECT_EQ(removed_calls, 0);
    EXPECT_EQ(remaining_calls, 1);
    removed.reset();
    EXPECT_TRUE(weak.expired());
    ASSERT_TRUE(window.remove_plugin(remaining));
    EXPECT_TRUE(window._data->update.empty());
    EXPECT_TRUE(window._data->plugins.empty());
}
TEST(WindowPluginRemovalTest, RejectsADifferentObjectWithTheSameId) {
    vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
    int actual_calls = 0, impostor_calls = 0;
    auto actual = window.add_plugin<CountingUpdatePlugin>(actual_calls);
    auto impostor = std::make_shared<CountingUpdatePlugin>(&window, impostor_calls);
    impostor->id = actual->id;
    EXPECT_FALSE(window.remove_plugin(impostor));
    ASSERT_EQ(window._data->plugins.size(), 1u);
    EXPECT_EQ(window._data->plugins.at(actual->id), actual);
    ASSERT_EQ(window._data->update.size(), 1u);
    EXPECT_EQ(window._data->update.front(), actual);
}
TEST(WindowPluginRemovalTest, RemovesPluginsWithoutAnUpdateSubscription) {
    vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
    auto plugin = window.add_plugin<TestPlugin>(23);
    EXPECT_TRUE(window._data->update.empty());
    EXPECT_TRUE(window.remove_plugin(plugin));
    EXPECT_TRUE(window._data->plugins.empty());
    EXPECT_FALSE(window.remove_plugin(plugin));
}
} // namespace

TEST(WindowPluginRemovalTest, NullPluginDoesNotChangeRegistrations) {
    vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
    auto plugin = window.add_plugin<TestPlugin>(23);
    EXPECT_FALSE(window.remove_plugin(std::shared_ptr<TestPlugin>{}));
    ASSERT_EQ(window._data->plugins.size(), 1u);
    EXPECT_EQ(window._data->plugins.at(plugin->id), plugin);
}

#include <type_traits>
namespace {
static_assert(std::has_virtual_destructor_v<vsl::PureWindow>);
static_assert(std::has_virtual_destructor_v<vsl::PureWindow::OnUpdate>);
struct TrackedWindow : vsl::PureWindow {
    bool &destroyed;
    TrackedWindow(std::shared_ptr<WindowData> data, bool &flag) : PureWindow(std::move(data)), destroyed(flag) {}
    ~TrackedWindow() { destroyed = true; }
};
struct TrackedUpdate : vsl::PureWindow::OnUpdate {
    bool &destroyed;
    explicit TrackedUpdate(bool &flag) : destroyed(flag) {}
    ~TrackedUpdate() { destroyed = true; }
    bool onUpdate(vsl::PureWindow) override { return true; }
};
TEST(WindowInterfaceLifetimeTest, BaseOwnersRunBothDerivedDestructors) {
    bool window_destroyed = false, update_destroyed = false;
    std::unique_ptr<vsl::PureWindow> window =
        std::make_unique<TrackedWindow>(std::make_shared<vsl::PureWindow::WindowData>(), window_destroyed);
    std::unique_ptr<vsl::PureWindow::OnUpdate> update = std::make_unique<TrackedUpdate>(update_destroyed);
    window.reset();
    update.reset();
    EXPECT_TRUE(window_destroyed);
    EXPECT_TRUE(update_destroyed);
}
TEST(WindowInterfaceLifetimeTest, CopiesShareDataAndMovesTransferIt) {
    static_assert(std::is_nothrow_move_constructible_v<vsl::PureWindow>);
    static_assert(std::is_nothrow_move_assignable_v<vsl::PureWindow>);
    auto data = std::make_shared<vsl::PureWindow::WindowData>();
    vsl::PureWindow original(data), copied(original);
    EXPECT_EQ(copied._data, data);
    vsl::PureWindow moved(std::move(original));
    EXPECT_FALSE(original._data);
    EXPECT_EQ(moved._data, data);
    original = copied;
    vsl::PureWindow assigned(std::make_shared<vsl::PureWindow::WindowData>());
    assigned = std::move(moved);
    EXPECT_FALSE(moved._data);
    EXPECT_EQ(assigned._data, data);
    EXPECT_EQ(original._data, data);
}
TEST_F(WindowIntegrationTest, DeletingADerivedWrapperRetainsTheSharedNativeWindow) {
    bool destroyed = false;
    std::unique_ptr<vsl::PureWindow> owner = std::make_unique<TrackedWindow>(window->_data, destroyed);
    owner.reset();
    EXPECT_TRUE(destroyed);
    EXPECT_TRUE(static_cast<bool>(*window));
}
} // namespace
