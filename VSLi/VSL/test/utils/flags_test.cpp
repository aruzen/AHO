#include <gtest/gtest.h>
#include <VSL/utils/flags.hpp>

namespace {
    enum class Bits : unsigned {
        NONE = 0,
        READ = 1,
        WRITE = 2,
        EXECUTE = 4
    };
    ___VSL_EXPAND_FLAGS_OPERATOR(Bits)
    static_assert(contain<Bits::READ | Bits::WRITE, Bits::READ>());
    static_assert(!contain<Bits::READ, Bits::WRITE>());
    static_assert(contain<Bits::NONE, Bits::NONE>());

    TEST(FlagsTest, BinaryOperatorsPreserveIndividualBits) {
        EXPECT_EQ(Bits::READ | Bits::WRITE, static_cast<Bits>(3));
        EXPECT_EQ((Bits::READ | Bits::WRITE) & Bits::WRITE, Bits::WRITE);
        EXPECT_EQ((Bits::READ | Bits::WRITE) ^ Bits::READ, Bits::WRITE);
    }
    TEST(FlagsTest, AssignmentOperatorsMutateAndReturnOriginalObject) {
        auto flags = Bits::READ;
        EXPECT_EQ(&(flags |= Bits::WRITE), &flags);
        EXPECT_EQ(flags, static_cast<Bits>(3));
        EXPECT_EQ(&(flags &= Bits::WRITE), &flags);
        EXPECT_EQ(flags, Bits::WRITE);
        EXPECT_EQ(&(flags ^= Bits::WRITE), &flags);
        EXPECT_EQ(flags, Bits::NONE);
    }
    TEST(FlagsTest, ContainRequiresAllRequestedBits) {
        EXPECT_TRUE(contain(Bits::READ | Bits::WRITE, Bits::READ));
        EXPECT_FALSE(contain(Bits::READ, Bits::READ | Bits::WRITE));
        EXPECT_TRUE(contain(Bits::READ, Bits::NONE));
    }
} // namespace
