#include <gtest/gtest.h>
#include <VSL/utils/flags.hpp>

namespace {
    enum class Bits : unsigned {
        None = 0,
        Read = 1,
        Write = 2,
        Execute = 4
    };
    ___VSL_EXPAND_FLAGS_OPERATOR(Bits)
    static_assert(contain<Bits::Read | Bits::Write, Bits::Read>());
    static_assert(!contain<Bits::Read, Bits::Write>());
    static_assert(contain<Bits::None, Bits::None>());

    TEST(FlagsTest, BinaryOperatorsPreserveIndividualBits) {
        EXPECT_EQ(Bits::Read | Bits::Write, static_cast<Bits>(3));
        EXPECT_EQ((Bits::Read | Bits::Write) & Bits::Write, Bits::Write);
        EXPECT_EQ((Bits::Read | Bits::Write) ^ Bits::Read, Bits::Write);
    }
    TEST(FlagsTest, AssignmentOperatorsMutateAndReturnOriginalObject) {
        auto flags = Bits::Read;
        EXPECT_EQ(&(flags |= Bits::Write), &flags);
        EXPECT_EQ(flags, static_cast<Bits>(3));
        EXPECT_EQ(&(flags &= Bits::Write), &flags);
        EXPECT_EQ(flags, Bits::Write);
        EXPECT_EQ(&(flags ^= Bits::Write), &flags);
        EXPECT_EQ(flags, Bits::None);
    }
    TEST(FlagsTest, ContainRequiresAllRequestedBits) {
        EXPECT_TRUE(contain(Bits::Read | Bits::Write, Bits::Read));
        EXPECT_FALSE(contain(Bits::Read, Bits::Read | Bits::Write));
        EXPECT_TRUE(contain(Bits::Read, Bits::None));
    }
} // namespace
