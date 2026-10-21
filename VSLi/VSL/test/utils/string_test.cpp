#include <gtest/gtest.h>
#include <VSL/utils/string.hpp>
#include <cstdlib>
#include "support/scoped_environment.hpp"

namespace {
    using vsl_test::ScopedEnvironment;
    class LoggingTest : public testing::Test {
        vsl::LoggerT _previous = nullptr;

    protected:
        inline static std::string MESSAGES;
        static void Capture(const char* text) {
            MESSAGES += text;
        }
        void SetUp() override {
            _previous = vsl::LOGGER;
            MESSAGES.clear();
            vsl::LOGGER = Capture;
        }
        void TearDown() override {
            vsl::LOGGER = _previous;
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
        EXPECT_EQ(MESSAGES, "value=42");
        vsl::loggingln<true>("ok=", true);
        EXPECT_EQ(MESSAGES, "value=42ok=true\n");
    }
    TEST_F(LoggingTest, NullLoggerIsSafe) {
        vsl::LOGGER = nullptr;
        vsl::logging<true>("ignored");
        vsl::loggingln<true>("ignored");
        EXPECT_TRUE(MESSAGES.empty());
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
        EXPECT_TRUE(MESSAGES.empty());
        vsl::logging<true>(value);
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(MESSAGES, "formatted");
    }
    TEST_F(LoggingTest, DisabledValidationDoesNotRequireStreamInsertion) {
        struct Unprintable {};
        vsl::logging<false>(Unprintable{});
        vsl::loggingln<false>(Unprintable{});
        EXPECT_TRUE(MESSAGES.empty());
    }
    TEST_F(LoggingTest, DisabledValidationDoesNotInvokeLogger) {
        vsl::logging<false>("ignored");
        vsl::loggingln<false>("ignored");
        EXPECT_TRUE(MESSAGES.empty());
    }
} // namespace
