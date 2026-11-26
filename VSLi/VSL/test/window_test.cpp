#include "../src/private/VSL/lifetime/window_access.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::WindowAccess;
}

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
        vsl::PureWindow shared(WindowAccess::FromState(WindowAccess::State(*(window))));
        EXPECT_TRUE(shared.operator==(*window));
        EXPECT_EQ(shared.identity(), window->identity());
        EXPECT_TRUE(window->resize(96, 72));
        int width = 0, height = 0;
        glfwGetWindowSize(static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle), &width, &height);
        EXPECT_EQ(width, 96);
        EXPECT_EQ(height, 72);
        EXPECT_TRUE(vsl::PureWindow::NoCheckUpdate());
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_TRUE(window->close());
        EXPECT_FALSE(static_cast<bool>(*window));
        EXPECT_TRUE(WindowAccess::Registry().empty());
    }

    TEST_F(WindowIntegrationTest, AddsAndRemovesPluginByIdentity) {
        auto plugin = window->add_plugin<TestPlugin>(17);
        EXPECT_EQ(plugin->value, 17);
        EXPECT_EQ(WindowAccess::State(*(window))->plugins.at(*window->plugin_id(plugin)), plugin);
        EXPECT_TRUE(window->remove_plugin(plugin));
        EXPECT_FALSE(window->remove_plugin(plugin));
    }

    TEST_F(WindowIntegrationTest, WindowConstructorsInstallQuietCloseOnce) {
        vsl::Window shared(WindowAccess::FromState(WindowAccess::State(*(window))));
        EXPECT_EQ(WindowAccess::State(shared), WindowAccess::State(*(window)));
        ASSERT_TRUE(WindowAccess::State(shared)->plugins.contains(vsl::window_plugin::QuietClose::QUIET_CLOSE_USED_ID));
        vsl::Window another(WindowAccess::FromState(WindowAccess::State(shared)));
        EXPECT_EQ(WindowAccess::State(another)->update.size(), 1u);
        vsl::Window separate("another hidden window", 32, 24);
        EXPECT_TRUE(static_cast<bool>(separate));
        EXPECT_EQ(separate.name(), "another hidden window");
        EXPECT_TRUE(separate.close());
    }
} // namespace

namespace {
    struct CountingUpdatePlugin : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
        int& calls;

        CountingUpdatePlugin(vsl::PureWindow*, int& calls, std::optional<unsigned char> id = std::nullopt) : Plugin(id), calls(calls) {}

        bool on_update(vsl::PureWindow) override {
            ++calls;
            return true;
        }
    };

    TEST(WindowPluginRemovalTest, RemovesTheUpdateSubscriptionAndReleasesItsOwnership) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int removed_calls = 0, remaining_calls = 0;
        auto removed = window.add_plugin<CountingUpdatePlugin>(removed_calls);
        auto remaining = window.add_plugin<CountingUpdatePlugin>(remaining_calls);
        std::weak_ptr<CountingUpdatePlugin> weak = removed;
        ASSERT_EQ(WindowAccess::State(window)->update.size(), 2u);
        ASSERT_TRUE(window.remove_plugin(removed));
        EXPECT_FALSE(window.remove_plugin(removed));
        ASSERT_EQ(WindowAccess::State(window)->update.size(), 1u);
        EXPECT_EQ(WindowAccess::State(window)->update.front(), window.plugin_id(remaining));
        for (const auto& subscriber : WindowAccess::State(window)->update)
            EXPECT_TRUE(window.plugin_as<vsl::PureWindow::OnUpdate>(subscriber)->on_update(window));
        EXPECT_EQ(removed_calls, 0);
        EXPECT_EQ(remaining_calls, 1);
        removed.reset();
        EXPECT_TRUE(weak.expired());
        ASSERT_TRUE(window.remove_plugin(remaining));
        EXPECT_TRUE(WindowAccess::State(window)->update.empty());
        EXPECT_TRUE(WindowAccess::State(window)->plugins.empty());
    }

    TEST(WindowPluginRemovalTest, RejectsADifferentObjectWithTheSameId) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int actual_calls = 0, impostor_calls = 0;
        auto actual = window.add_plugin<CountingUpdatePlugin>(actual_calls);
        auto impostor = std::make_shared<CountingUpdatePlugin>(&window, impostor_calls, window.plugin_id(actual));
        EXPECT_FALSE(window.remove_plugin(impostor));
        ASSERT_EQ(WindowAccess::State(window)->plugins.size(), 1u);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(*window.plugin_id(actual)), actual);
        ASSERT_EQ(WindowAccess::State(window)->update.size(), 1u);
        EXPECT_EQ(WindowAccess::State(window)->update.front(), window.plugin_id(actual));
    }

    TEST(WindowPluginRemovalTest, RemovesPluginsWithoutAnUpdateSubscription) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        auto plugin = window.add_plugin<TestPlugin>(23);
        EXPECT_TRUE(WindowAccess::State(window)->update.empty());
        EXPECT_TRUE(window.remove_plugin(plugin));
        EXPECT_TRUE(WindowAccess::State(window)->plugins.empty());
        EXPECT_FALSE(window.remove_plugin(plugin));
    }
} // namespace

TEST(WindowPluginRemovalTest, NullPluginDoesNotChangeRegistrations) {
    vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
    auto plugin = window.add_plugin<TestPlugin>(23);
    EXPECT_FALSE(window.remove_plugin(std::shared_ptr<TestPlugin>{}));
    ASSERT_EQ(WindowAccess::State(window)->plugins.size(), 1u);
    EXPECT_EQ(WindowAccess::State(window)->plugins.at(*window.plugin_id(plugin)), plugin);
}

namespace {
    struct UpdateActionPlugin : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
        std::function<bool(vsl::PureWindow)> action;

        UpdateActionPlugin(vsl::PureWindow*, std::function<bool(vsl::PureWindow)> action)
            : action(std::move(action)) {}
        bool on_update(vsl::PureWindow window) override { return action(std::move(window)); }
    };

    struct ScopedUpdateWindow {
        vsl::PureWindow window{"VSL update mutation test", 32, 24};

        ~ScopedUpdateWindow() {
            if (WindowAccess::State(window) && WindowAccess::State(window)->window_handle)
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
                const auto previous_capacity = WindowAccess::Registry().capacity();
                do {
                    created.push_back(std::make_unique<ScopedUpdateWindow>());
                    EXPECT_TRUE(static_cast<bool>(created.back()->window));
                    created.back()->window.add_plugin<CountingUpdatePlugin>(created_calls);
                } while (WindowAccess::Registry().capacity() == previous_capacity);
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
        vsl::PureWindow window(WindowAccess::FromState(std::shared_ptr<vsl::PureWindow::WindowData>{}));
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_FALSE(window.close());
    }

    TEST(WindowValidityTest, MovedFromWrappersRemainInvalidAndReusable) {
        auto data = std::make_shared<vsl::PureWindow::WindowData>();
        vsl::PureWindow source(WindowAccess::FromState(data));
        vsl::PureWindow destination(std::move(source));
        ASSERT_FALSE(WindowAccess::State(source));
        EXPECT_FALSE(static_cast<bool>(source));
        EXPECT_FALSE(source.close());
        EXPECT_EQ(WindowAccess::State(destination), data);
        source = destination;
        EXPECT_EQ(WindowAccess::State(source), data);
        EXPECT_FALSE(static_cast<bool>(source)); // The shared data has no native window.
        vsl::PureWindow assigned(WindowAccess::FromState(std::shared_ptr<vsl::PureWindow::WindowData>{}));
        assigned = std::move(source);
        ASSERT_FALSE(WindowAccess::State(source));
        EXPECT_FALSE(static_cast<bool>(source));
        EXPECT_EQ(WindowAccess::State(assigned), data);
    }
} // namespace

#include <VSL/exceptions.hpp>

namespace {
    TEST_F(WindowIntegrationTest, FailedCreationLeavesExistingWindowUsable) {
        const auto before = WindowAccess::Registry();
        std::optional<vsl::PureWindow> failed;
        try {
            failed.emplace("invalid width", 0, 24);
            ADD_FAILURE() << "Window creation failure was silently accepted";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(std::string(error.what()).find("GLFW"), std::string::npos);
        }
        EXPECT_EQ(WindowAccess::Registry(), before);
        // Clean up the failed object left by the old implementation, too.
        if (failed)
            failed->close();
        EXPECT_TRUE(static_cast<bool>(*window));
        EXPECT_TRUE(window->resize(80, 60));
        int width = 0, height = 0;
        glfwGetWindowSize(static_cast<GLFWwindow*>(WindowAccess::State(*(window))->window_handle), &width, &height);
        EXPECT_EQ(width, 80);
        EXPECT_EQ(height, 60);
        EXPECT_TRUE(vsl::PureWindow::NoCheckUpdate());
    }

    TEST_F(WindowIntegrationTest, FailedFirstCreationLeavesNoRegistrationAndAllowsRetry) {
        ASSERT_TRUE(window->close());
        ASSERT_TRUE(WindowAccess::Registry().empty());
        std::optional<vsl::PureWindow> failed;
        EXPECT_THROW(failed.emplace("invalid first window", 32, 0), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(WindowAccess::Registry().empty());
        if (failed)
            failed->close();
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        vsl::PureWindow retry("valid retry", 32, 24);
        EXPECT_TRUE(static_cast<bool>(retry));
        EXPECT_EQ(WindowAccess::Registry().size(), 1u);
        EXPECT_TRUE(retry.close());
        EXPECT_TRUE(WindowAccess::Registry().empty());
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

    ASSERT_TRUE(WindowAccess::Registry().empty());
    vsl::lifetime::GLFWLease::Drain();
    glfwInitHint(GLFW_PLATFORM, -1); // GLFW rejects an unsupported platform ID during initialization.
    std::optional<vsl::PureWindow> failed;
    try {
        failed.emplace("cannot initialize GLFW", 32, 24);
        ADD_FAILURE() << "GLFW initialization failure was silently accepted";
    } catch (const vsl::exceptions::RuntimeException& error) {
        EXPECT_NE(std::string(error.what()).find("GLFW"), std::string::npos);
    }
    EXPECT_TRUE(WindowAccess::Registry().empty());
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

        TrackedWindow(std::shared_ptr<WindowData> data, bool& flag) : PureWindow(WindowAccess::FromState(std::move(data))), destroyed(flag) {}
        ~TrackedWindow() { destroyed = true; }
    };

    struct TrackedUpdate : vsl::PureWindow::OnUpdate {
        bool& destroyed;

        explicit TrackedUpdate(bool& flag) : destroyed(flag) {}
        ~TrackedUpdate() { destroyed = true; }
        bool on_update(vsl::PureWindow) override { return true; }
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
        vsl::PureWindow original(WindowAccess::FromState(data)), copied(original);
        EXPECT_EQ(WindowAccess::State(copied), data);
        vsl::PureWindow moved(std::move(original));
        EXPECT_FALSE(WindowAccess::State(original));
        EXPECT_EQ(WindowAccess::State(moved), data);
        original = copied;
        vsl::PureWindow assigned(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        assigned = std::move(moved);
        EXPECT_FALSE(WindowAccess::State(moved));
        EXPECT_EQ(WindowAccess::State(assigned), data);
        EXPECT_EQ(WindowAccess::State(original), data);
    }

    TEST_F(WindowIntegrationTest, DeletingADerivedWrapperRetainsTheSharedNativeWindow) {
        bool destroyed = false;
        std::unique_ptr<vsl::PureWindow> owner = std::make_unique<TrackedWindow>(WindowAccess::State(*(window)), destroyed);
        owner.reset();
        EXPECT_TRUE(destroyed);
        EXPECT_TRUE(static_cast<bool>(*window));
    }
} // namespace

namespace {
    struct WindowCloseErrorCapture {
        inline static unsigned CALLS = 0;
        GLFWerrorfun previous;

        WindowCloseErrorCapture() : previous(glfwSetErrorCallback([](int, const char*) { ++CALLS; })) { CALLS = 0; }

        ~WindowCloseErrorCapture() {
            glfwSetErrorCallback(previous);
            glfwGetError(nullptr);
        }
    };

    TEST(WindowValidityTest, EmptyNativeDataCloseAndDestroyDoNotCallUninitializedGlfw) {
        ASSERT_TRUE(WindowAccess::Registry().empty());
        glfwTerminate();
        WindowCloseErrorCapture errors;
        auto data = std::make_shared<vsl::PureWindow::WindowData>();
        vsl::PureWindow window(WindowAccess::FromState(data));
        EXPECT_FALSE(window.close());
        data->destroy();
        data->destroy();
        EXPECT_EQ(WindowCloseErrorCapture::CALLS, 0u);
        EXPECT_TRUE(WindowAccess::Registry().empty());
    }

    TEST_F(WindowIntegrationTest, RepeatedCloseThroughAliasesAndDataDoesNotReenterTerminatedGlfw) {
        auto data = WindowAccess::State(*(window));
        vsl::PureWindow alias(WindowAccess::FromState(data));
        WindowCloseErrorCapture errors;
        ASSERT_TRUE(window->close());
        EXPECT_FALSE(static_cast<bool>(alias));
        EXPECT_EQ(data->window_handle, nullptr);
        EXPECT_TRUE(WindowAccess::Registry().empty());
        EXPECT_FALSE(alias.close());
        EXPECT_FALSE(window->close());
        data->destroy();
        EXPECT_EQ(WindowCloseErrorCapture::CALLS, 0u);
    }

    TEST_F(WindowIntegrationTest, RepeatedCloseLeavesAnotherRegisteredWindowUsable) {
        ScopedUpdateWindow other;
        int calls = 0;
        other.window.add_plugin<CountingUpdatePlugin>(calls);
        WindowCloseErrorCapture errors;
        ASSERT_TRUE(window->close());
        EXPECT_FALSE(window->close());
        WindowAccess::State(*(window))->destroy();
        EXPECT_TRUE(static_cast<bool>(other.window));
        ASSERT_EQ(WindowAccess::Registry().size(), 1u);
        EXPECT_EQ(WindowAccess::Registry().front(), WindowAccess::State(other.window));
        EXPECT_TRUE(other.window.resize(48, 24));
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(WindowCloseErrorCapture::CALLS, 0u);
    }

    TEST_F(WindowIntegrationTest, ClosingAnOldAliasAfterRestartKeepsTheNewWindowRegistered) {
        vsl::PureWindow old(WindowAccess::FromState(WindowAccess::State(*(window))));
        ASSERT_TRUE(window->close());
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        ScopedUpdateWindow next;
        WindowCloseErrorCapture errors;
        EXPECT_FALSE(old.close());
        WindowAccess::State(old)->destroy();
        EXPECT_TRUE(static_cast<bool>(next.window));
        ASSERT_EQ(WindowAccess::Registry().size(), 1u);
        EXPECT_EQ(WindowAccess::Registry().front(), WindowAccess::State(next.window));
        EXPECT_TRUE(next.window.resize(48, 24));
        EXPECT_EQ(WindowCloseErrorCapture::CALLS, 0u);
    }
} // namespace

#include <limits>

namespace {
    struct RegistrationListener : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
        int& calls;

        RegistrationListener(vsl::PureWindow*, int& calls, std::optional<unsigned char> fixed_id = std::nullopt) : Plugin(fixed_id), calls(calls) {}

        bool on_update(vsl::PureWindow) override {
            ++calls;
            return true;
        }
    };

    struct RegistrationPlugin : vsl::PureWindow::Plugin {
        explicit RegistrationPlugin(vsl::PureWindow*, std::optional<unsigned char> fixed_id = std::nullopt) : Plugin(fixed_id) {}
    };

    TEST(WindowPluginRegistrationTest, AutomaticIdsSkipRegisteredFixedIds) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int fixed_calls = 0, automatic_calls = 0;
        auto fixed = window.add_plugin<RegistrationListener>(fixed_calls, 1);
        auto automatic = window.add_plugin<RegistrationListener>(automatic_calls);
        auto next = window.add_plugin<RegistrationListener>(automatic_calls);
        EXPECT_NE(*window.plugin_id(automatic), *window.plugin_id(fixed));
        EXPECT_NE(*window.plugin_id(next), *window.plugin_id(fixed));
        EXPECT_NE(*window.plugin_id(next), *window.plugin_id(automatic));
        ASSERT_EQ(WindowAccess::State(window)->plugins.size(), 3u);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(*window.plugin_id(fixed)), fixed);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(*window.plugin_id(automatic)), automatic);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(*window.plugin_id(next)), next);
        for (const auto& listener : WindowAccess::State(window)->update)
            EXPECT_TRUE(window.plugin_as<vsl::PureWindow::OnUpdate>(listener)->on_update(window));
        EXPECT_EQ(fixed_calls, 1);
        EXPECT_EQ(automatic_calls, 2);
    }

    TEST(WindowPluginRegistrationTest, WrappingAutomaticIdsReuseTheVacantSlotWithoutReplacingSurvivors) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        const auto capacity = static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1u;
        std::vector<std::shared_ptr<RegistrationPlugin>> plugins;
        for (unsigned int i = 0; i < capacity; ++i)
            plugins.push_back(window.add_plugin<RegistrationPlugin>());
        const auto vacant_id = *window.plugin_id(plugins[37]);
        ASSERT_TRUE(window.remove_plugin(plugins[37]));
        auto replacement = window.add_plugin<RegistrationPlugin>();
        EXPECT_EQ(*window.plugin_id(replacement), vacant_id);
        ASSERT_EQ(WindowAccess::State(window)->plugins.size(), capacity);
        for (const auto& plugin : plugins)
            if (plugin != plugins[37])
                EXPECT_EQ(WindowAccess::State(window)->plugins.at(*window.plugin_id(plugin)), plugin);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(vacant_id), replacement);
    }

    TEST(WindowPluginRegistrationTest, ExhaustionReportsAnErrorAndPreservesRegistrationsAndSubscriptions) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int calls = 0;
        const auto capacity = static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1u;
        for (unsigned int i = 0; i < capacity; ++i)
            window.add_plugin<RegistrationListener>(calls);
        const auto registrations = WindowAccess::State(window)->plugins;
        const auto subscriptions = WindowAccess::State(window)->update;
        const auto next_id = WindowAccess::State(window)->plugins_next_id;
        try {
            window.add_plugin<RegistrationListener>(calls);
            FAIL() << "Exhausted plugin IDs must not replace an existing registration";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find("plugin ID"), std::string::npos);
        }
        EXPECT_EQ(WindowAccess::State(window)->plugins, registrations);
        EXPECT_EQ(WindowAccess::State(window)->update, subscriptions);
        EXPECT_EQ(WindowAccess::State(window)->plugins_next_id, next_id);
    }

    TEST(WindowPluginRegistrationTest, ReplacingAFixedIdAlsoReplacesItsUpdateSubscription) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int previous_calls = 0, replacement_calls = 0;
        auto previous = window.add_plugin<RegistrationListener>(previous_calls, 11);
        auto replacement = window.add_plugin<RegistrationListener>(replacement_calls, 11);
        ASSERT_EQ(WindowAccess::State(window)->plugins.size(), 1u);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(11), replacement);
        ASSERT_EQ(WindowAccess::State(window)->update.size(), 1u);
        EXPECT_TRUE(window.plugin_as<vsl::PureWindow::OnUpdate>(11)->on_update(window));
        EXPECT_EQ(previous_calls, 0);
        EXPECT_EQ(replacement_calls, 1);
        EXPECT_FALSE(window.remove_plugin(previous));
        EXPECT_TRUE(window.remove_plugin(replacement));
        EXPECT_TRUE(WindowAccess::State(window)->plugins.empty());
        EXPECT_TRUE(WindowAccess::State(window)->update.empty());
    }
} // namespace

namespace {
    struct ConstructActionPlugin : vsl::PureWindow::Plugin {
        bool& destroyed;

        ConstructActionPlugin(vsl::PureWindow*, bool& destroyed, std::function<void()> action) : destroyed(destroyed) { action(); }
        ~ConstructActionPlugin() override { destroyed = true; }
    };

    struct ReleaseActionPlugin : vsl::PureWindow::Plugin {
        std::function<void()> action;

        ReleaseActionPlugin(vsl::PureWindow*, unsigned char id, std::function<void()> action) : Plugin(id), action(std::move(action)) {}
        ~ReleaseActionPlugin() override { action(); }
    };

    TEST(WindowPluginRegistrationTest, ExplicitZeroIsAFixedRegistrationAndAutomaticIdsSkipIt) {
        auto window = WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>());
        const auto fixed = window.add_plugin<RegistrationPlugin>(0);
        const auto automatic = window.add_plugin<RegistrationPlugin>();
        EXPECT_EQ(window.plugin_id(fixed), 0);
        EXPECT_EQ(window.plugin_id(automatic), 1);
        const auto replacement = window.add_plugin<RegistrationPlugin>(0);
        EXPECT_FALSE(window.plugin_id(fixed));
        EXPECT_EQ(window.plugin_id(replacement), 0);
    }

    TEST(WindowPluginRegistrationTest, ConstructorDeletingTheReceiverDoesNotPublishAndRollsBackItsPlugin) {
        const auto state = std::make_shared<vsl::PureWindow::WindowData>();
        auto window = std::make_unique<vsl::PureWindow>(WindowAccess::FromState(state));
        bool destroyed = false;
        EXPECT_THROW(window->add_plugin<ConstructActionPlugin>(destroyed, [&] { window.reset(); }), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(destroyed);
        EXPECT_TRUE(state->plugins.empty());
        EXPECT_TRUE(state->update.empty());
    }

    TEST(WindowPluginRegistrationTest, ConstructorReplacingTheReceiverDoesNotPublishIntoEitherWindow) {
        const auto state = std::make_shared<vsl::PureWindow::WindowData>();
        auto window = WindowAccess::FromState(state);
        auto next = WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>());
        bool destroyed = false;
        EXPECT_THROW(window.add_plugin<ConstructActionPlugin>(destroyed, [&] { window = next; }), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(destroyed);
        EXPECT_EQ(window, next);
        EXPECT_TRUE(state->plugins.empty());
        EXPECT_TRUE(next.plugin_snapshot().empty());
    }

    TEST(WindowPluginRegistrationTest, ReplacedPluginDestructorCanReplaceTheCommittedRegistrationAgain) {
        auto window = WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>());
        std::shared_ptr<RegistrationPlugin> reentrant;
        window.add_plugin<ReleaseActionPlugin>(7, [&] { reentrant = window.add_plugin<RegistrationPlugin>(7); });
        const auto outer = window.add_plugin<RegistrationPlugin>(7);
        ASSERT_TRUE(reentrant);
        EXPECT_EQ(window.plugin(7), reentrant);
        EXPECT_FALSE(window.plugin_id(outer));
        EXPECT_TRUE(WindowAccess::State(window)->update.empty());
    }

    TEST(WindowIdentityTest, DifferentSlotsWithTheSameOwnerHaveDifferentIdentities) {
        const auto states = std::make_shared<std::array<vsl::PureWindow::WindowData, 2>>();
        const auto first = WindowAccess::FromState({states, &(*states)[0]});
        const auto second = WindowAccess::FromState({states, &(*states)[1]});
        EXPECT_NE(first.identity(), second.identity());
        EXPECT_EQ(first.identity(), vsl::PureWindow(first).identity());
    }

    TEST_F(WindowIntegrationTest, ClosedLogicalWindowCannotBeLockedWhileItsStateAndNativeOwnerSurvive) {
        const auto weak = window->weak();
        const auto identity = window->identity();
        const auto os = window->window_owner();
        const auto state = WindowAccess::State(*window);
        EXPECT_EQ(weak.lock(), *window);
        EXPECT_TRUE(window->close());
        EXPECT_TRUE(weak.expired());
        EXPECT_FALSE(weak.lock());
        EXPECT_TRUE(os);
        EXPECT_EQ(window->identity(), identity);
        EXPECT_TRUE(state->plugins.empty());
    }

    TEST_F(WindowIntegrationTest, ClosingDuringPluginConstructionRejectsPublication) {
        bool destroyed = false;
        EXPECT_THROW(window->add_plugin<ConstructActionPlugin>(destroyed, [&] { window->close(); }), std::invalid_argument);
        EXPECT_TRUE(destroyed);
        EXPECT_TRUE(window->plugin_snapshot().empty());
        EXPECT_FALSE(*window);
    }
} // namespace

namespace {
    TEST(WindowPluginRegistrationTest, FixedIdReplacementCanRemoveAndRestoreAnUpdateSubscription) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int calls = 0;
        auto previous = window.add_plugin<RegistrationListener>(calls, 11);
        std::weak_ptr<RegistrationListener> weak = previous;
        auto passive = window.add_plugin<RegistrationPlugin>(11);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(11), passive);
        EXPECT_TRUE(WindowAccess::State(window)->update.empty());
        previous.reset();
        EXPECT_TRUE(weak.expired());
        auto replacement = window.add_plugin<RegistrationListener>(calls, 11);
        ASSERT_EQ(WindowAccess::State(window)->plugins.size(), 1u);
        ASSERT_EQ(WindowAccess::State(window)->update.size(), 1u);
        EXPECT_TRUE(window.plugin_as<vsl::PureWindow::OnUpdate>(11)->on_update(window));
        EXPECT_EQ(calls, 1);
        EXPECT_TRUE(window.remove_plugin(replacement));
        EXPECT_TRUE(WindowAccess::State(window)->update.empty());
    }

    TEST(WindowPluginRegistrationTest, AFullRegistryStillAllowsExplicitFixedIdReplacement) {
        vsl::PureWindow window(WindowAccess::FromState(std::make_shared<vsl::PureWindow::WindowData>()));
        int calls = 0;
        const auto capacity = static_cast<unsigned int>(std::numeric_limits<unsigned char>::max()) + 1u;
        for (unsigned int i = 0; i < capacity; ++i)
            window.add_plugin<RegistrationListener>(calls);
        const auto previous = WindowAccess::State(window)->plugins.at(11);
        auto replacement = window.add_plugin<RegistrationListener>(calls, 11);
        EXPECT_NE(previous, replacement);
        EXPECT_EQ(WindowAccess::State(window)->plugins.at(11), replacement);
        EXPECT_EQ(WindowAccess::State(window)->plugins.size(), capacity);
        EXPECT_EQ(WindowAccess::State(window)->update.size(), capacity);
        EXPECT_FALSE(window.plugin_id(previous));
    }
} // namespace
