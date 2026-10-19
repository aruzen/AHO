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

namespace {
struct UpdateActionPlugin : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
    std::function<bool(vsl::PureWindow)> action;
    UpdateActionPlugin(vsl::PureWindow *, std::function<bool(vsl::PureWindow)> action)
        : action(std::move(action)) {}
    bool onUpdate(vsl::PureWindow window) override { return action(std::move(window)); }
};
struct ScopedUpdateWindow {
    vsl::PureWindow window{"VSL update mutation test", 32, 24};
    ~ScopedUpdateWindow() { if (window._data && window._data->window_handle) window.close(); }
};
TEST_F(WindowIntegrationTest, UpdateSkipsRemovedListenersAndDefersNewListenersOnTheCurrentWindow) {
    int first_calls = 0, removed_calls = 0, added_calls = 0;
    bool changed = false;
    std::shared_ptr<CountingUpdatePlugin> removed, added;
    window->add_plugin<UpdateActionPlugin>([&](vsl::PureWindow current) {
        ++first_calls;
        if (!changed) {
            changed = true;
            EXPECT_TRUE(current.remove_plugin(removed));
            added = current.add_plugin<CountingUpdatePlugin>(added_calls);
        }
        return false;
    });
    removed = window->add_plugin<CountingUpdatePlugin>(removed_calls);
    EXPECT_FALSE(vsl::PureWindow::Update());
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(removed_calls, 0);
    EXPECT_EQ(added_calls, 0);
    EXPECT_FALSE(vsl::PureWindow::Update());
    EXPECT_EQ(first_calls, 2);
    EXPECT_EQ(removed_calls, 0);
    EXPECT_EQ(added_calls, 1);
}
TEST_F(WindowIntegrationTest, UpdatingListenerStaysAliveUntilItsSelfRemovalCallbackReturns) {
    int follower_calls = 0;
    bool alive_during_callback = false;
    std::weak_ptr<UpdateActionPlugin> weak;
    std::shared_ptr<UpdateActionPlugin> self;
    self = window->add_plugin<UpdateActionPlugin>([&](vsl::PureWindow current) {
        EXPECT_TRUE(current.remove_plugin(self));
        self.reset();
        alive_during_callback = !weak.expired();
        return true;
    });
    weak = self;
    window->add_plugin<CountingUpdatePlugin>(follower_calls);
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_TRUE(alive_during_callback);
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(follower_calls, 1);
}
TEST_F(WindowIntegrationTest, ClosingTheCurrentWindowSkipsItsOtherCallbacksAndStillUpdatesAnotherWindow) {
    int stale_calls = 0, other_calls = 0;
    ScopedUpdateWindow other;
    ASSERT_TRUE(static_cast<bool>(other.window));
    window->add_plugin<UpdateActionPlugin>([](vsl::PureWindow current) { return current.close(); });
    window->add_plugin<CountingUpdatePlugin>(stale_calls);
    other.window.add_plugin<CountingUpdatePlugin>(other_calls);
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(stale_calls, 0);
    EXPECT_EQ(other_calls, 1);
    EXPECT_FALSE(static_cast<bool>(*window));
}
TEST_F(WindowIntegrationTest, WindowsAddedDuringUpdateStartReceivingCallbacksOnTheNextTick) {
    int first_calls = 0, other_calls = 0, created_calls = 0;
    ScopedUpdateWindow other;
    std::vector<std::unique_ptr<ScopedUpdateWindow>> created;
    window->add_plugin<UpdateActionPlugin>([&](vsl::PureWindow) {
        ++first_calls;
        if (created.empty()) {
            const auto previous_capacity = vsl::PureWindow::WINDOWS.capacity();
            do {
                created.push_back(std::make_unique<ScopedUpdateWindow>());
                EXPECT_TRUE(static_cast<bool>(created.back()->window));
                created.back()->window.add_plugin<CountingUpdatePlugin>(created_calls);
            } while (vsl::PureWindow::WINDOWS.capacity() == previous_capacity);
        }
        return true;
    });
    other.window.add_plugin<CountingUpdatePlugin>(other_calls);
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(other_calls, 1);
    EXPECT_EQ(created_calls, 0);
    ASSERT_FALSE(created.empty());
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(first_calls, 2);
    EXPECT_EQ(other_calls, 2);
    EXPECT_EQ(created_calls, static_cast<int>(created.size()));
}
} // namespace

namespace {
TEST_F(WindowIntegrationTest, AListenerAddedToALaterWindowRunsWhenThatWindowBeginsUpdating) {
    int calls = 0;
    ScopedUpdateWindow other;
    std::shared_ptr<CountingUpdatePlugin> added;
    window->add_plugin<UpdateActionPlugin>([&](vsl::PureWindow) {
        if (!added) added = other.window.add_plugin<CountingUpdatePlugin>(calls);
        return true;
    });
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(vsl::PureWindow::Update());
    EXPECT_EQ(calls, 2);
}
} // namespace
