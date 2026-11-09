#include <VSL/lifetime/facade_call.hpp>
#include <gtest/gtest.h>
#include <memory>

namespace {
    TEST(FacadeCallTest, NestedInvalidationOnlyAffectsExistingCallsForTheSameSubject) {
        int first = 0, second = 0;
        vsl::lifetime::FacadeCall outer(&first);
        vsl::lifetime::FacadeCall independent(&second);
        {
            vsl::lifetime::FacadeCall nested(&first);
            vsl::lifetime::FacadeCall::Invalidate(&first);
            EXPECT_FALSE(outer.alive());
            EXPECT_FALSE(nested.alive());
            EXPECT_TRUE(independent.alive());
            vsl::lifetime::FacadeCall fresh(&first);
            EXPECT_TRUE(fresh.alive());
        }
        vsl::lifetime::FacadeCall::Invalidate(&second);
        EXPECT_FALSE(independent.alive());
        EXPECT_FALSE(outer.alive());
    }

    TEST(FacadeCallTest, GuardCleanupDoesNotAccessAReleasedSubjectOrLeaveItsCallInTheStack) {
        {
            auto subject = std::make_unique<int>(7);
            vsl::lifetime::FacadeCall call(subject.get());
            vsl::lifetime::FacadeCall::Invalidate(subject.get());
            subject.reset();
            EXPECT_FALSE(call.alive());
        }
        int next = 0;
        vsl::lifetime::FacadeCall call(&next);
        EXPECT_TRUE(call.alive());
        vsl::lifetime::FacadeCall::Invalidate(&next);
        EXPECT_FALSE(call.alive());
    }
} // namespace
