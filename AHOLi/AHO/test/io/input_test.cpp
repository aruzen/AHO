#include <AHO/io/input.hpp>
#if defined(AHO_CONTRACT_INPUT_MOUSE_INTEGER)
constexpr aho::input::MouseID first_button(0), last_button(7);
static_assert(first_button.code() == aho::input::MouseCode::LeftClick);
static_assert(last_button.code() == aho::input::MouseCode::Button8);
#elif defined(AHO_LINK_INPUT_KEY_NAME)
int main() { return aho::input::KeyID(aho::input::KeyCode::A).name().empty(); }
#elif defined(AHO_LINK_INPUT_MOUSE_NAME)
int main() { return aho::input::MouseID(aho::input::MouseCode::LeftClick).name() != "LeftClick"; }
#else
#include <gtest/gtest.h>
#include <set>
#include <memory>
#include <type_traits>
namespace {
    using namespace aho::input::abstract;
    static_assert(std::has_virtual_destructor_v<Input<int>>);
    static_assert(std::has_virtual_destructor_v<Updatable>);
    static_assert(std::has_virtual_destructor_v<Button>);
    static_assert(std::has_virtual_destructor_v<Cursor>);
    static_assert(std::has_virtual_destructor_v<Text>);
    static_assert(std::has_virtual_destructor_v<Characters>);
    struct TrackedInput : Input<int>, Updatable {
        int& destructions;
        std::unique_ptr<int> resource = std::make_unique<int>(7);
        explicit TrackedInput(int& destructions) : destructions(destructions) {}
        ~TrackedInput() { ++destructions; }
        int state() override { return *resource; }
    };
    template<class Base>
    class InputDestructionTest : public testing::Test {};
    using InputBases = testing::Types<Input<int>, Updatable>;
    TYPED_TEST_SUITE(InputDestructionTest, InputBases);
    TYPED_TEST(InputDestructionTest, DeletingThroughEitherBaseDestroysTheDerivedObjectOnce) {
        int destructions = 0;
        {
            std::unique_ptr<TypeParam> input = std::make_unique<TrackedInput>(destructions);
            auto* state = dynamic_cast<Input<int>*>(input.get());
            ASSERT_NE(state, nullptr);
            EXPECT_EQ(state->state(), 7);
            EXPECT_EQ(destructions, 0);
        }
        EXPECT_EQ(destructions, 1);
    }
    TEST(InputInterfaceTest, DefaultUpdatePreservesStateAndDoesNotThrow) {
        struct DefaultInput : aho::input::abstract::Input<int>, aho::input::abstract::Updatable {
            int value = 7;
            int state() override { return value; }
        } input;
        EXPECT_NO_THROW(input.update());
        EXPECT_EQ(input.state(), 7);
    }
    TEST(InputCodeTest, TypedAndIntegerKeyConstructorsPreserveCodes) {
        const aho::input::KeyID typed(aho::input::KeyCode::A), integer(65);
        EXPECT_EQ(typed.code(), aho::input::KeyCode::A);
        EXPECT_EQ(integer.code(), aho::input::KeyCode::A);
        EXPECT_EQ(static_cast<int>(aho::input::KeyID(-1).code()), -1);
        EXPECT_EQ(aho::input::MouseID(aho::input::MouseCode::RightClick).code(), aho::input::MouseCode::Button2);
    }
    TEST(InputCodeTest, IntegerMouseConstructorPreservesEveryButtonCode) {
        for (int code = 0; code < 8; ++code) {
            const aho::input::MouseID id(code);
            EXPECT_EQ(id.code(), aho::input::MouseCodes[code]);
            EXPECT_EQ(static_cast<int>(id.code()), code);
        }
    }
    TEST(InputCodeNameTest, KeyboardUsesBackendNamesAndHandlesMissingNames) {
        using namespace aho::input;
        EXPECT_EQ(KeyID(KeyCode::A).name(), "test-layout-a");
        EXPECT_EQ(KeyID(KeyCode::B).name(), "test-layout-b");
        EXPECT_TRUE(KeyID(KeyCode::Enter).name().empty());
    }
    TEST(InputCodeNameTest, MouseNamesCoverEveryButtonAndUnknownCodes) {
        using namespace aho::input;
        constexpr const char* names[] = {
            "LeftClick", "RightClick", "MiddleButton", "Button4",
            "Button5", "Button6", "Button7", "Button8"};
        for (int code = 0; code < 8; ++code) {
            EXPECT_EQ(MouseID(code).name(), names[code]) << code;
        }
        EXPECT_TRUE(MouseID(255).name().empty());
    }
    TEST(InputCodeTest, EnumeratedKeysAndMouseButtonsAreUniqueAndComplete) {
        using namespace aho::input;
        const std::set<KeyCode> keys(KeyCodes.begin(), KeyCodes.end());
        EXPECT_EQ(keys.size(), KeyCodes.size());
        EXPECT_TRUE(keys.contains(KeyCode::A));
        EXPECT_TRUE(keys.contains(KeyCode::F25));
        EXPECT_TRUE(keys.contains(KeyCode::Menu));
        const std::set<MouseCode> buttons(MouseCodes.begin(), MouseCodes.end());
        EXPECT_EQ(buttons.size(), 8u);
        EXPECT_EQ(MouseCode::LeftClick, MouseCode::Button1);
        EXPECT_EQ(MouseCode::RightClick, MouseCode::Button2);
        EXPECT_EQ(MouseCode::LastButton, MouseCode::Button8);
    }
} // namespace
#endif
