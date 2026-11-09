#include <VSL/lifetime/phase_liveness.hpp>
#include <VSL/vulkan/phase.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>

namespace {
    TEST(PhaseLivenessTest, NestedGuardsSurviveUnwindingAndObserveDestructionWithoutOwningThePhase) {
        auto phase = std::make_unique<vsl::DefaultPhase>(vsl::CommandManager{nullptr}, vsl::SwapchainAccessor{},
                                                         std::nullopt, std::nullopt, std::nullopt, true);
        vsl::lifetime::PhaseCall outer(*phase);
        EXPECT_TRUE(outer.alive());
        try {
            vsl::lifetime::PhaseCall inner(*phase);
            EXPECT_TRUE(inner.alive());
            throw std::runtime_error("unwind guard");
        } catch (const std::runtime_error&) {
        }
        EXPECT_TRUE(outer.alive());
        vsl::lifetime::PhaseCall inner(*phase);
        phase.reset();
        EXPECT_FALSE(outer.alive());
        EXPECT_FALSE(inner.alive());
    }
} // namespace
