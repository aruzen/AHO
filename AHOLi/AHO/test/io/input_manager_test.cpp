#include <AHO/io/input_manager.hpp>
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
template<class T> struct TypedCounter : Counter {
    TypedCounter(void* context, T value) : Counter(context, value) {}
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
    EXPECT_EQ(first->state(), 4); EXPECT_EQ(second->state(), 0);
    manager.update();
    EXPECT_EQ(first->state(), 5); EXPECT_EQ(second->state(), 1);
    EXPECT_TRUE(static_cast<bool>(manager));
    EXPECT_EQ(first->state(), 6); EXPECT_EQ(second->state(), 2);
}
TEST_F(InputManagerTest, AdaptableInputsReceiveManagerWithoutExtraUpdateRegistration) {
    aho::InputManager manager(window.handle());
    const auto group = manager.get<Group>(7);
    EXPECT_EQ(group->context, window.handle()); EXPECT_EQ(group->manager, &manager); EXPECT_EQ(group->value, 7);
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
}
