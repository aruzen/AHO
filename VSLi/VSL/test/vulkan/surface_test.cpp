#include "support/window_fixture.hpp"
namespace {
    using SurfaceWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(SurfaceWindowIntegrationTest, CreatesSurfaceForWindowAndRetainsInstance) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_EQ(surface->_data->vulkan, instance);
        EXPECT_EQ(surface->_data->window, window->_data->window_handle);
        const auto capabilities = physical._data->device.getSurfaceCapabilitiesKHR(surface->_data->surface);
        EXPECT_GE(capabilities.maxImageExtent.width, 64u);
        EXPECT_GE(capabilities.maxImageExtent.height, 48u);
        EXPECT_FALSE(physical._data->device.getSurfaceFormatsKHR(surface->_data->surface).empty());
    }
} // namespace
