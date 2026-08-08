#include <VSL/window.hpp>
#include <VSL/vulkan/phase.hpp>

#include <gtest/gtest.h>

TEST(VSLOrderRegression, DISABLED_DefaultPhaseSubmitBeforeSetupShouldNotTouchNullFence)
{
    // This documents an order-sensitive lifecycle bug:
    // DefaultPhase::submit() assumes several Vulkan-backed fields are valid.
    // A future regression test should build a minimal fake/real phase and assert
    // that submit-before-setup fails cleanly instead of dereferencing invalid state.
}

TEST(VSLOrderRegression, DISABLED_PureWindowCloseShouldBeIdempotent)
{
    // PureWindow::close() currently calls WindowData::destroy() and destroy()
    // calls glfwDestroyWindow(window_handle) without an explicit null guard.
    // A real GLFW integration test should verify that close-close is safe.
}

TEST(VSLOrderRegression, DISABLED_SurfaceCreationFailureShouldNotLeavePartiallyConstructedSurface)
{
    // Surface construction logs and returns on glfwCreateWindowSurface failure.
    // A future test should force surface creation failure and assert that the
    // object cannot later destroy or expose an invalid VkSurfaceKHR.
}
