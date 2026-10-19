#include <gtest/gtest.h>
#include <VSL/utils/string.hpp>
#include <cstdlib>
#include "support/scoped_environment.hpp"

namespace {
using vsl_test::ScopedEnvironment;
class LoggingTest : public testing::Test {
    vsl::logger_t previous_ = nullptr;

  protected:
    inline static std::string messages;
    static void capture(const char *text) {
        messages += text;
    }
    void SetUp() override {
        previous_ = vsl::logger;
        messages.clear();
        vsl::logger = capture;
    }
    void TearDown() override {
        vsl::logger = previous_;
    }
};
TEST(EnvironmentExpansionTest, PreservesPlainEmptyAndInvalidPlaceholders) {
    EXPECT_EQ(vsl::expand_environments(""), "");
    EXPECT_EQ(vsl::expand_environments("plain/path"), "plain/path");
    EXPECT_EQ(vsl::expand_environments("$NAME ${1INVALID} ${}"), "$NAME ${1INVALID} ${}");
}
TEST(EnvironmentExpansionTest, ReplacesRepeatedAndAdjacentVariablesWithoutRecursion) {
    ScopedEnvironment first("VSL_TEST_FIRST", "alpha");
    ScopedEnvironment second("VSL_TEST_SECOND", "${VSL_TEST_FIRST}");
    EXPECT_EQ(vsl::expand_environments("/${VSL_TEST_FIRST}/${VSL_TEST_FIRST}"), "/alpha/alpha");
    EXPECT_EQ(vsl::expand_environments("${VSL_TEST_FIRST}${VSL_TEST_FIRST}"), "alphaalpha");
    EXPECT_EQ(vsl::expand_environments("${VSL_TEST_SECOND}"), "${VSL_TEST_FIRST}");
}
TEST(EnvironmentExpansionTest, RemovesUnsetAndEmptyVariables) {
    ScopedEnvironment missing("VSL_TEST_MISSING", nullptr);
    ScopedEnvironment empty("VSL_TEST_EMPTY", "");
    EXPECT_EQ(vsl::expand_environments("a${VSL_TEST_MISSING}b${VSL_TEST_EMPTY}c"), "abc");
}
TEST_F(LoggingTest, JoinConcatenatesValuesAndFormatsBooleans) {
    std::stringstream stream;
    vsl::helper::join(stream);
    EXPECT_TRUE(stream.str().empty());
    vsl::helper::join(stream, "value=", 4, '/', true, '/', false);
    EXPECT_EQ(stream.str(), "value=4/true/false");
}
TEST_F(LoggingTest, LoggingAndLogginglnSendExpectedText) {
    vsl::logging<true>("value=", 42);
    EXPECT_EQ(messages, "value=42");
    vsl::loggingln<true>("ok=", true);
    EXPECT_EQ(messages, "value=42ok=true\n");
}
TEST_F(LoggingTest, NullLoggerIsSafe) {
    vsl::logger = nullptr;
    vsl::logging<true>("ignored");
    vsl::loggingln<true>("ignored");
    EXPECT_TRUE(messages.empty());
}
struct CountedFormatting {
    int* calls;
    friend std::ostream& operator<<(std::ostream& stream, const CountedFormatting& value) {
        ++*value.calls;
        return stream << "formatted";
    }
};
TEST_F(LoggingTest, DisabledValidationDoesNotFormatArguments) {
    int calls = 0;
    const CountedFormatting value{&calls};
    vsl::logging<false>(value);
    vsl::loggingln<false>(value);
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(messages.empty());
    vsl::logging<true>(value);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(messages, "formatted");
}
TEST_F(LoggingTest, DisabledValidationDoesNotRequireStreamInsertion) {
    struct Unprintable {};
    vsl::logging<false>(Unprintable{});
    vsl::loggingln<false>(Unprintable{});
    EXPECT_TRUE(messages.empty());
}
TEST_F(LoggingTest, DisabledValidationDoesNotInvokeLogger) {
    vsl::logging<false>("ignored");
    vsl::loggingln<false>("ignored");
    EXPECT_TRUE(messages.empty());
}
} // namespace
