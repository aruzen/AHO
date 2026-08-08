#include <aho/io/input.hpp>
#include <aho/io/input_manager.hpp>

#include <gtest/gtest.h>

namespace {
class CountingInput final : public aho::input::abstract::Updatable {
public:
    CountingInput(void *context, int *updates)
        : context(context)
        , updates(updates)
    {
    }

    void update() override
    {
        ++(*updates);
    }

    void *context = nullptr;
    int *updates = nullptr;
};
}

TEST(AHOInput, CodeIdKeepsKeyCode)
{
    const aho::input::KeyID id{aho::input::KeyCode::A};

    EXPECT_EQ(id.code(), aho::input::KeyCode::A);
}

TEST(AHOInput, CodeIdKeepsMouseCode)
{
    const aho::input::MouseID id{aho::input::MouseCode::RightClick};

    EXPECT_EQ(id.code(), aho::input::MouseCode::RightClick);
}

TEST(AHOInput, KeyAndMouseCodeTablesContainExpectedCounts)
{
    EXPECT_EQ(aho::input::KeyCodes.size(), 120u);
    EXPECT_EQ(aho::input::MouseCodes.size(), 8u);
    EXPECT_EQ(aho::input::KeyCodes.front(), aho::input::KeyCode::Space);
    EXPECT_EQ(aho::input::MouseCodes.front(), aho::input::MouseCode::LeftClick);
}

TEST(AHOInputManager, UpdatesTrackedInputs)
{
    int updates = 0;
    aho::InputManager manager{reinterpret_cast<void *>(0x1234)};

    auto input = manager.get<CountingInput>(&updates);

    ASSERT_EQ(input->context, reinterpret_cast<void *>(0x1234));
    EXPECT_EQ(manager.update_targets.size(), 1u);

    manager.update();

    EXPECT_EQ(updates, 1);
}

TEST(AHOInputManager, RemovesExpiredInputsOnUpdate)
{
    int updates = 0;
    aho::InputManager manager{nullptr};

    {
        auto input = manager.get<CountingInput>(&updates);
        ASSERT_EQ(manager.update_targets.size(), 1u);
    }

    manager.update();

    EXPECT_EQ(manager.update_targets.size(), 0u);
    EXPECT_EQ(updates, 0);
}
