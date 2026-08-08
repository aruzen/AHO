#include <VSL/exceptions.hpp>
#include <VSL/utils/string.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace {
void set_test_env(const char *name, const char *value)
{
#ifdef _MSC_VER
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

std::string logged;

void capture_logger(const char *message)
{
    logged += message;
}
}

TEST(VSLString, ExpandsExistingEnvironmentVariables)
{
    set_test_env("VSL_TEST_TOKEN", "expanded");

    EXPECT_EQ(vsl::expand_environments("prefix-${VSL_TEST_TOKEN}-suffix"), "prefix-expanded-suffix");
}

TEST(VSLString, RemovesMissingEnvironmentVariables)
{
    EXPECT_EQ(vsl::expand_environments("prefix-${VSL_MISSING_TOKEN}-suffix"), "prefix--suffix");
}

TEST(VSLLogging, JoinsValuesAndFormatsBool)
{
    auto *previous = vsl::logger;
    logged.clear();
    vsl::logger = capture_logger;

    vsl::logging("value=", 42, ", flag=", true);
    vsl::loggingln(" done");

    vsl::logger = previous;

    EXPECT_EQ(logged, "value=42, flag=true done\n");
}

TEST(VSLException, RuntimeExceptionIncludesTypeMessageAndTrace)
{
    const vsl::exceptions::RuntimeException error{"Type", "message", "trace"};
    const std::string what = error.what();

    EXPECT_NE(what.find("RuntimeException by Type"), std::string::npos);
    EXPECT_NE(what.find("message"), std::string::npos);
    EXPECT_NE(what.find("trace"), std::string::npos);
}
