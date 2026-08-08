#include <VSL/window.hpp>
#include <VSL/vulkan/phase.hpp>

#include <gtest/gtest.h>

TEST(VSLOrderRegression, DISABLED_DefaultPhaseSubmitBeforeSetupShouldNotTouchNullFence)
{
    // DefaultPhase::submit() now fails cleanly when called before setup.
    // A future Vulkan integration test should verify this against real phase
    // objects and invalid semaphore/fence combinations.
}

TEST(VSLOrderRegression, DISABLED_PureWindowCloseShouldBeIdempotent)
{
    // PureWindow::close() and WindowData::destroy() now guard null handles.
    // A real GLFW integration test should verify that close-close is safe
    // against an actual created window.
}

TEST(VSLOrderRegression, DISABLED_SurfaceCreationFailureShouldNotLeavePartiallyConstructedSurface)
{
    // Surface construction now throws on invalid inputs and failed
    // glfwCreateWindowSurface calls. A future test should force surface
    // creation failure and assert that no invalid VkSurfaceKHR is exposed.
}
