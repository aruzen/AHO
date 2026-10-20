#include "support/window_fixture.hpp"
namespace {
    using WindowIntegrationTest = vsl_test::WindowIntegrationTest;
    struct TestPlugin : vsl::PureWindow::Plugin {
        int value;
        TestPlugin(vsl::PureWindow*, int value) : value(value) {}
    };
    TEST_F(WindowIntegrationTest, NamesSharedConstructorsComparisonsResizeAndClose) {
        EXPECT_EQ(window->name(), "VSL hidden integration test");
        EXPECT_TRUE(static_cast<bool>(*window));
        vsl::PureWindow shared(window->_data);
        EXPECT_TRUE(shared.operator==(*window));
        EXPECT_TRUE(shared == window->_data.get());
        EXPECT_TRUE(window->resize(96, 72));
        int width = 0, height = 0;
        glfwGetWindowSize(static_cast<GLFWwindow*>(window->_data->window_handle), &width, &height);
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
        int& calls;
        CountingUpdatePlugin(vsl::PureWindow*, int& calls) : calls(calls) {}
        bool onUpdate(vsl::PureWindow) override {
            ++calls;
            return true;
        }
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
        for (const auto& subscriber : window._data->update)
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
        UpdateActionPlugin(vsl::PureWindow*, std::function<bool(vsl::PureWindow)> action)
            : action(std::move(action)) {}
        bool onUpdate(vsl::PureWindow window) override { return action(std::move(window)); }
    };
    struct ScopedUpdateWindow {
        vsl::PureWindow window{"VSL update mutation test", 32, 24};
        ~ScopedUpdateWindow() {
            if (window._data && window._data->window_handle)
                window.close();
        }
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
            if (!added)
                added = other.window.add_plugin<CountingUpdatePlugin>(calls);
            return true;
        });
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(calls, 1);
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(calls, 2);
    }
} // namespace

namespace {
    TEST(WindowValidityTest, NullSharedDataIsInvalidAndCanBeClosed) {
        vsl::PureWindow window(std::shared_ptr<vsl::PureWindow::WindowData>{});
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_FALSE(window.close());
    }
    TEST(WindowValidityTest, MovedFromWrappersRemainInvalidAndReusable) {
        auto data = std::make_shared<vsl::PureWindow::WindowData>();
        vsl::PureWindow source(data);
        vsl::PureWindow destination(std::move(source));
        ASSERT_FALSE(source._data);
        EXPECT_FALSE(static_cast<bool>(source));
        EXPECT_FALSE(source.close());
        EXPECT_EQ(destination._data, data);
        source = destination;
        EXPECT_EQ(source._data, data);
        EXPECT_FALSE(static_cast<bool>(source)); // The shared data has no native window.
        vsl::PureWindow assigned(std::shared_ptr<vsl::PureWindow::WindowData>{});
        assigned = std::move(source);
        ASSERT_FALSE(source._data);
        EXPECT_FALSE(static_cast<bool>(source));
        EXPECT_EQ(assigned._data, data);
    }
} // namespace

#include <VSL/exceptions.hpp>
namespace {
    TEST_F(WindowIntegrationTest, FailedCreationLeavesExistingWindowUsable) {
        const auto before = vsl::PureWindow::WINDOWS;
        std::optional<vsl::PureWindow> failed;
        try {
            failed.emplace("invalid width", 0, 24);
            ADD_FAILURE() << "Window creation failure was silently accepted";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(std::string(error.what()).find("GLFW"), std::string::npos);
        }
        EXPECT_EQ(vsl::PureWindow::WINDOWS, before);
        // Clean up the failed object left by the old implementation, too.
        if (failed)
            failed->close();
        EXPECT_TRUE(static_cast<bool>(*window));
        EXPECT_TRUE(window->resize(80, 60));
        int width = 0, height = 0;
        glfwGetWindowSize(static_cast<GLFWwindow*>(window->_data->window_handle), &width, &height);
        EXPECT_EQ(width, 80);
        EXPECT_EQ(height, 60);
        EXPECT_TRUE(vsl::PureWindow::NoCheckUpdate());
    }
    TEST_F(WindowIntegrationTest, FailedFirstCreationLeavesNoRegistrationAndAllowsRetry) {
        ASSERT_TRUE(window->close());
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        std::optional<vsl::PureWindow> failed;
        EXPECT_THROW(failed.emplace("invalid first window", 32, 0), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        if (failed)
            failed->close();
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        vsl::PureWindow retry("valid retry", 32, 24);
        EXPECT_TRUE(static_cast<bool>(retry));
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), 1u);
        EXPECT_TRUE(retry.close());
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
    }
} // namespace

#if defined(GLFW_PLATFORM)
TEST(WindowInitializationTest, FailedGlfwInitializationThrowsWithoutRegisteringAWindow) {
    struct RestorePlatform {
        ~RestorePlatform() {
            glfwTerminate();
            glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
        }
    } restore;
    ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
    glfwInitHint(GLFW_PLATFORM, -1); // GLFW rejects an unsupported platform ID during initialization.
    std::optional<vsl::PureWindow> failed;
    try {
        failed.emplace("cannot initialize GLFW", 32, 24);
        ADD_FAILURE() << "GLFW initialization failure was silently accepted";
    } catch (const vsl::exceptions::RuntimeException& error) {
        EXPECT_NE(std::string(error.what()).find("GLFW"), std::string::npos);
    }
    EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
    if (failed)
        failed->close();
}
#endif

#include <type_traits>
namespace {
    static_assert(std::has_virtual_destructor_v<vsl::PureWindow>);
    static_assert(std::has_virtual_destructor_v<vsl::PureWindow::OnUpdate>);
    struct TrackedWindow : vsl::PureWindow {
        bool& destroyed;
        TrackedWindow(std::shared_ptr<WindowData> data, bool& flag) : PureWindow(std::move(data)), destroyed(flag) {}
        ~TrackedWindow() { destroyed = true; }
    };
    struct TrackedUpdate : vsl::PureWindow::OnUpdate {
        bool& destroyed;
        explicit TrackedUpdate(bool& flag) : destroyed(flag) {}
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

#include <limits>
namespace {
    struct RegistrationListener : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
        int& calls;
        RegistrationListener(vsl::PureWindow*, int& calls, unsigned char fixed_id = 0) : calls(calls) {
            id = fixed_id;
        }
        bool onUpdate(vsl::PureWindow) override { ++calls; return true; }
    };
    struct RegistrationPlugin : vsl::PureWindow::Plugin {
        explicit RegistrationPlugin(vsl::PureWindow*, unsigned char fixed_id = 0) { id = fixed_id; }
    };
    TEST(WindowPluginRegistrationTest, AutomaticIdsSkipRegisteredFixedIds) {
        vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
        int fixed_calls = 0, automatic_calls = 0;
        auto fixed = window.add_plugin<RegistrationListener>(fixed_calls, 1);
        auto automatic = window.add_plugin<RegistrationListener>(automatic_calls);
        auto next = window.add_plugin<RegistrationListener>(automatic_calls);
        EXPECT_NE(automatic->id, fixed->id);
        EXPECT_NE(next->id, fixed->id);
        EXPECT_NE(next->id, automatic->id);
        ASSERT_EQ(window._data->plugins.size(), 3u);
        EXPECT_EQ(window._data->plugins.at(fixed->id), fixed);
        EXPECT_EQ(window._data->plugins.at(automatic->id), automatic);
        EXPECT_EQ(window._data->plugins.at(next->id), next);
        for (const auto& listener : window._data->update)
            EXPECT_TRUE(listener->onUpdate(window));
        EXPECT_EQ(fixed_calls, 1);
        EXPECT_EQ(automatic_calls, 2);
    }
    TEST(WindowPluginRegistrationTest, WrappingAutomaticIdsReuseTheVacantSlotWithoutReplacingSurvivors) {
        vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
        const auto capacity = static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1u;
        std::vector<std::shared_ptr<RegistrationPlugin>> plugins;
        for (unsigned int i = 0; i < capacity; ++i)
            plugins.push_back(window.add_plugin<RegistrationPlugin>());
        const auto vacant_id = plugins[37]->id;
        ASSERT_TRUE(window.remove_plugin(plugins[37]));
        auto replacement = window.add_plugin<RegistrationPlugin>();
        EXPECT_EQ(replacement->id, vacant_id);
        ASSERT_EQ(window._data->plugins.size(), capacity);
        for (const auto& plugin : plugins)
            if (plugin->id != vacant_id)
                EXPECT_EQ(window._data->plugins.at(plugin->id), plugin);
        EXPECT_EQ(window._data->plugins.at(vacant_id), replacement);
    }
    TEST(WindowPluginRegistrationTest, ExhaustionReportsAnErrorAndPreservesRegistrationsAndSubscriptions) {
        vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
        int calls = 0;
        const auto capacity = static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1u;
        for (unsigned int i = 0; i < capacity; ++i)
            window.add_plugin<RegistrationListener>(calls);
        const auto registrations = window._data->plugins;
        const auto subscriptions = window._data->update;
        const auto next_id = window._data->plugins_next_id;
        try {
            window.add_plugin<RegistrationListener>(calls);
            FAIL() << "Exhausted plugin IDs must not replace an existing registration";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find("plugin ID"), std::string::npos);
        }
        EXPECT_EQ(window._data->plugins, registrations);
        EXPECT_EQ(window._data->update, subscriptions);
        EXPECT_EQ(window._data->plugins_next_id, next_id);
    }
    TEST(WindowPluginRegistrationTest, ReplacingAFixedIdAlsoReplacesItsUpdateSubscription) {
        vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
        int previous_calls = 0, replacement_calls = 0;
        auto previous = window.add_plugin<RegistrationListener>(previous_calls, 11);
        auto replacement = window.add_plugin<RegistrationListener>(replacement_calls, 11);
        ASSERT_EQ(window._data->plugins.size(), 1u);
        EXPECT_EQ(window._data->plugins.at(11), replacement);
        ASSERT_EQ(window._data->update.size(), 1u);
        EXPECT_TRUE(window._data->update.front()->onUpdate(window));
        EXPECT_EQ(previous_calls, 0);
        EXPECT_EQ(replacement_calls, 1);
        EXPECT_FALSE(window.remove_plugin(previous));
        EXPECT_TRUE(window.remove_plugin(replacement));
        EXPECT_TRUE(window._data->plugins.empty());
        EXPECT_TRUE(window._data->update.empty());
    }
} // namespace

namespace {
    TEST(WindowPluginRegistrationTest, FixedIdReplacementCanRemoveAndRestoreAnUpdateSubscription) {
        vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
        int calls = 0;
        auto previous = window.add_plugin<RegistrationListener>(calls, 11);
        std::weak_ptr<RegistrationListener> weak = previous;
        auto passive = window.add_plugin<RegistrationPlugin>(11);
        EXPECT_EQ(window._data->plugins.at(11), passive);
        EXPECT_TRUE(window._data->update.empty());
        previous.reset();
        EXPECT_TRUE(weak.expired());
        auto replacement = window.add_plugin<RegistrationListener>(calls, 11);
        ASSERT_EQ(window._data->plugins.size(), 1u);
        ASSERT_EQ(window._data->update.size(), 1u);
        EXPECT_TRUE(window._data->update.front()->onUpdate(window));
        EXPECT_EQ(calls, 1);
        EXPECT_TRUE(window.remove_plugin(replacement));
        EXPECT_TRUE(window._data->update.empty());
    }
    TEST(WindowPluginRegistrationTest, AFullRegistryStillAllowsExplicitFixedIdReplacement) {
        vsl::PureWindow window(std::make_shared<vsl::PureWindow::WindowData>());
        int calls = 0;
        const auto capacity = static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1u;
        for (unsigned int i = 0; i < capacity; ++i)
            window.add_plugin<RegistrationListener>(calls);
        const auto previous = window._data->plugins.at(11);
        auto replacement = window.add_plugin<RegistrationListener>(calls, 11);
        EXPECT_NE(previous, replacement);
        EXPECT_EQ(window._data->plugins.at(11), replacement);
        EXPECT_EQ(window._data->plugins.size(), capacity);
        EXPECT_EQ(window._data->update.size(), capacity);
        const auto retired = std::dynamic_pointer_cast<vsl::PureWindow::OnUpdate>(previous);
        EXPECT_EQ(std::ranges::count(window._data->update, retired), 0);
    }
} // namespace
