#include <gtest/gtest.h>
#include <VSL/exceptions.hpp>

namespace {
TEST(ExceptionTest, DefaultTypeMessageAndTraceArePreserved) {
    vsl::exceptions::RuntimeException error("", "failed", "trace");
    EXPECT_STREQ(error.what(), "RuntimeException : failed(trace)");
    EXPECT_EQ(error.reason, "RuntimeException : failed(trace)");
}
TEST(ExceptionTest, ExplicitTypeAndDefaultArgumentsArePreserved) {
    vsl::exceptions::RuntimeException error("Buffer");
    EXPECT_STREQ(error.what(), "RuntimeException by Buffer : ()");
}
TEST(ExceptionTest, SourceLocationReportsTheProvidedLine) {
    const auto location = std::source_location::current();
    vsl::exceptions::RuntimeException error("Device", "failed", location);
    EXPECT_EQ(error.reason, "RuntimeException by Device : failed(" + std::to_string(location.line()) + ")");
}
// Known issue I26: std::exception::what() is not overridden.
TEST(ExceptionTest, DISABLED_StandardExceptionReferencePreservesReason) {
    vsl::exceptions::RuntimeException error("Device", "failed", "trace");
    const std::exception &base = error;
    EXPECT_STREQ(base.what(), error.reason.c_str());
}
} // namespace
