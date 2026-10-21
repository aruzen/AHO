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
    TEST(ExceptionTest, StandardExceptionReferencePreservesReason) {
        vsl::exceptions::RuntimeException error("Device", "failed", "trace");
        const std::exception& base = error;
        EXPECT_STREQ(base.what(), error.reason.c_str());
    }
    TEST(ExceptionTest, ConstVSLExceptionReferenceProvidesTheReasonWithoutThrowing) {
        const vsl::exceptions::RuntimeException error("Buffer", "allocation failed", "trace");
        const vsl::exceptions::VSLException& base = error;
        static_assert(noexcept(base.what()));
        EXPECT_STREQ(base.what(), error.reason.c_str());
        EXPECT_EQ(base.what(), base.what());
    }
    TEST(ExceptionTest, StandardExceptionCatchReportsTheOriginalReason) {
        try {
            throw vsl::exceptions::RuntimeException("Device", "failed", "trace");
        } catch (const std::exception& error) {
            EXPECT_STREQ(error.what(), "RuntimeException by Device : failed(trace)");
            return;
        }
        FAIL() << "The runtime exception was not caught through std::exception.";
    }
} // namespace
