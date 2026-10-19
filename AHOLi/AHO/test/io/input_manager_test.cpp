#include <AHO/io/input_manager.hpp>
#include <functional>
#include "../support/input_backend.hpp"

namespace {
    struct Counter : aho::input::abstract::Input<int>, aho::input::abstract::Updatable {
        void* context;
        int updates;
        Counter(void* context, int initial = 0) : context(context), updates(initial) {}
        int state() override { return updates; }
        void update() override { ++updates; }
    };
    struct Group : aho::input::abstract::Adaptable {
        void* context;
        aho::InputManager* manager;
        int value;
        Group(void* context, aho::InputManager& manager, int value) : context(context), manager(&manager), value(value) {}
    };
    template<class T>
    struct TypedCounter : Counter {
        TypedCounter(void* context, T value) : Counter(context, value) {}
    };
    struct PassiveInput {
        void* context;
        int value;
        PassiveInput(void* context, int value) : context(context), value(value) {}
        int state() const { return value; }
    };
    template<class T>
    struct TypedPassiveInput {
        void* context;
        T value;
        TypedPassiveInput(void* context, T value) : context(context), value(value) {}
        T state() const { return value; }
    };
    struct UpdatingGroup : aho::input::abstract::Adaptable, aho::input::abstract::Updatable {
        void* context;
        aho::InputManager* manager;
        std::shared_ptr<Counter> child;
        int updates = 0;
        UpdatingGroup(void* context, aho::InputManager& manager, int initial)
            : context(context), manager(&manager), child(manager.get<Counter>(initial)) {}
        void update() override { ++updates; }
    };
    template<class T>
    struct TypedUpdatingGroup : UpdatingGroup {
        TypedUpdatingGroup(void* context, aho::InputManager& manager, T initial)
            : UpdatingGroup(context, manager, initial) {}
    };
    using InputManagerTest = aho_test::InputBackendTest;
    TEST_F(InputManagerTest, ConstructorsPreserveRawAndVslWindowHandles) {
        aho::InputManager raw(window.handle());
        EXPECT_EQ(raw.context, window.handle());
        auto data = std::make_shared<vsl::PureWindow::WindowData>();
        data->window_handle = window.handle();
        aho::InputManager wrapped{vsl::PureWindow(data)};
        EXPECT_EQ(wrapped.context, window.handle());
    }
    TEST_F(InputManagerTest, GetForwardsArgumentsAndUpdatesLiveInputs) {
        aho::InputManager manager(window.handle());
        const auto first = manager.get<Counter>(4), second = manager.get<Counter>();
        EXPECT_EQ(first->context, window.handle());
        EXPECT_EQ(first->state(), 4);
        EXPECT_EQ(second->state(), 0);
        manager.update();
        EXPECT_EQ(first->state(), 5);
        EXPECT_EQ(second->state(), 1);
        EXPECT_TRUE(static_cast<bool>(manager));
        EXPECT_EQ(first->state(), 6);
        EXPECT_EQ(second->state(), 2);
    }
    TEST_F(InputManagerTest, AdaptableInputsReceiveManagerWithoutExtraUpdateRegistration) {
        aho::InputManager manager(window.handle());
        const auto group = manager.get<Group>(7);
        EXPECT_EQ(group->context, window.handle());
        EXPECT_EQ(group->manager, &manager);
        EXPECT_EQ(group->value, 7);
        EXPECT_TRUE(manager.update_targets.empty());
    }
    TEST_F(InputManagerTest, TemplateInputFactoryForwardsArgumentsAndRegistersUpdates) {
        aho::InputManager manager(window.handle());
        const auto counter = manager.get<TypedCounter>(4);
        EXPECT_EQ(counter->context, window.handle());
        EXPECT_EQ(counter->state(), 4);
        manager.update();
        EXPECT_EQ(counter->state(), 5);
        EXPECT_EQ(manager.update_targets.size(), 1u);
    }
    TEST_F(InputManagerTest, RemovingExpiredInputDoesNotSkipFollowingLiveInput) {
        aho::InputManager manager(window.handle());
        auto expired = manager.get<Counter>();
        const auto live = manager.get<Counter>();
        expired.reset();
        manager.update();
        EXPECT_EQ(manager.update_targets.size(), 1u);
        EXPECT_EQ(live->state(), 1);
    }
    TEST_F(InputManagerTest, ConsecutiveExpiredInputsDoNotSkipSurvivorsOrLeaveSubscriptions) {
        aho::InputManager manager(window.handle());
        auto first_expired = manager.get<Counter>();
        auto second_expired = manager.get<Counter>();
        const auto first_live = manager.get<Counter>(10);
        auto middle_expired = manager.get<Counter>();
        const auto second_live = manager.get<Counter>(20);
        auto last_expired = manager.get<Counter>();
        first_expired.reset();
        second_expired.reset();
        middle_expired.reset();
        last_expired.reset();

        manager.update();
        ASSERT_EQ(manager.update_targets.size(), 2u);
        EXPECT_EQ(first_live->state(), 11);
        EXPECT_EQ(second_live->state(), 21);
        EXPECT_EQ(manager.update_targets[0].lock().get(), first_live.get());
        EXPECT_EQ(manager.update_targets[1].lock().get(), second_live.get());
        manager.update();
        EXPECT_EQ(first_live->state(), 12);
        EXPECT_EQ(second_live->state(), 22);
    }
    TEST_F(InputManagerTest, EmptyAndEntirelyExpiredListsCanBeUpdatedRepeatedly) {
        aho::InputManager manager(window.handle());
        manager.update();
        auto first = manager.get<Counter>();
        auto second = manager.get<Counter>();
        first.reset();
        second.reset();
        manager.update();
        EXPECT_TRUE(manager.update_targets.empty());
        manager.update();
        EXPECT_TRUE(manager.update_targets.empty());
    }
    TEST_F(InputManagerTest, PassiveInputsWithoutVirtualFunctionsDoNotRequireUpdateRegistration) {
        aho::InputManager manager(window.handle());
        const auto input = manager.get<PassiveInput>(7);
        EXPECT_EQ(input->context, window.handle());
        EXPECT_EQ(input->state(), 7);
        EXPECT_TRUE(manager.update_targets.empty());
        manager.update();
        EXPECT_EQ(input->state(), 7);
    }
    TEST_F(InputManagerTest, TemplatePassiveInputsForwardTheirArgumentsWithoutUpdateRegistration) {
        aho::InputManager manager(window.handle());
        const auto input = manager.get<TypedPassiveInput>(2.5);
        static_assert(std::same_as<std::remove_cv_t<decltype(input)>, std::shared_ptr<TypedPassiveInput<double>>>);
        EXPECT_EQ(input->context, window.handle());
        EXPECT_DOUBLE_EQ(input->state(), 2.5);
        EXPECT_TRUE(manager.update_targets.empty());
        manager.update();
        EXPECT_DOUBLE_EQ(input->state(), 2.5);
    }
    TEST_F(InputManagerTest, AdaptableUpdatableInputsRegisterBothGroupAndChildrenExactlyOnce) {
        aho::InputManager manager(window.handle());
        auto group = manager.get<UpdatingGroup>(7);
        EXPECT_EQ(group->context, window.handle());
        EXPECT_EQ(group->manager, &manager);
        EXPECT_EQ(manager.update_targets.size(), 2u);
        manager.update();
        EXPECT_EQ(group->updates, 1);
        EXPECT_EQ(group->child->state(), 8);
        EXPECT_TRUE(static_cast<bool>(manager));
        EXPECT_EQ(group->updates, 2);
        EXPECT_EQ(group->child->state(), 9);
        group.reset();
        manager.update();
        EXPECT_TRUE(manager.update_targets.empty());
    }
    TEST_F(InputManagerTest, TemplateAdaptableUpdatableInputsKeepIndependentChildLifetimes) {
        aho::InputManager manager(window.handle());
        auto group = manager.get<TypedUpdatingGroup>(3);
        static_assert(std::same_as<decltype(group), std::shared_ptr<TypedUpdatingGroup<int>>>);
        EXPECT_EQ(group->context, window.handle());
        EXPECT_EQ(group->manager, &manager);
        EXPECT_EQ(manager.update_targets.size(), 2u);
        const auto child = group->child;
        manager.update();
        EXPECT_EQ(group->updates, 1);
        EXPECT_EQ(child->state(), 4);
        group.reset();
        manager.update();
        ASSERT_EQ(manager.update_targets.size(), 1u);
        EXPECT_EQ(manager.update_targets[0].lock().get(), child.get());
        EXPECT_EQ(child->state(), 5);
    }

}

namespace {
    struct UpdateCallback : aho::input::abstract::Updatable {
        std::function<void()> callback;
        UpdateCallback(void*, std::function<void()> callback) : callback(std::move(callback)) {}
        void update() override { callback(); }
    };
    TEST_F(InputManagerTest, InputsRegisteredDuringUpdateStartOnTheNextUpdate) {
        aho::InputManager manager(window.handle());
        std::vector<std::shared_ptr<Counter>> children;
        bool spawned = false;
        auto creator = manager.get<UpdateCallback>([&] {
            if (spawned)
                return;
            spawned = true;
            const auto capacity = manager.update_targets.capacity();
            for (std::size_t i = 0; i <= capacity; ++i)
                children.push_back(manager.get<Counter>());
        });
        auto existing = manager.get<Counter>();
        manager.update();
        ASSERT_FALSE(children.empty());
        EXPECT_EQ(existing->state(), 1);
        for (const auto& child : children)
            EXPECT_EQ(child->state(), 0);
        manager.update();
        EXPECT_EQ(existing->state(), 2);
        for (const auto& child : children)
            EXPECT_EQ(child->state(), 1);
    }
    TEST_F(InputManagerTest, InputsDestroyedBeforeTheirTurnAreNotKeptAliveByTheUpdateSnapshot) {
        aho::InputManager manager(window.handle());
        std::shared_ptr<UpdateCallback> later;
        int calls = 0;
        auto first = manager.get<UpdateCallback>([&] { later.reset(); });
        later = manager.get<UpdateCallback>([&] { ++calls; });
        manager.update();
        EXPECT_EQ(calls, 0);
        ASSERT_EQ(manager.update_targets.size(), 1u);
        EXPECT_EQ(manager.update_targets.front().lock(), first);
    }
    TEST_F(InputManagerTest, ExpiredInputsRegisteredByACallbackAreRemovedAfterTheUpdate) {
        aho::InputManager manager(window.handle());
        auto creator = manager.get<UpdateCallback>([&] { (void)manager.get<Counter>(); });
        manager.update();
        ASSERT_EQ(manager.update_targets.size(), 1u);
        EXPECT_EQ(manager.update_targets.front().lock(), creator);
    }
} // namespace
