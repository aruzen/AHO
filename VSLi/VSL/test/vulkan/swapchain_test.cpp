#include "support/window_fixture.hpp"
namespace {
using SwapchainWindowIntegrationTest = vsl_test::WindowIntegrationTest;
struct BorrowedImageViews {
    vsl::SwapchainAccessor chain;
    ~BorrowedImageViews() {
        for (auto &image : chain.images) {
            chain._data->device->device.destroyImageView(image._data->view);
            image._data->view = nullptr;
        }
    }
};
TEST_F(SwapchainWindowIntegrationTest, CreatesImagesAndRecreatesUsingOldSwapchain) {
    make_surface();
    ASSERT_FALSE(HasFatalFailure());
    if (!physical._data->device.getSurfaceSupportKHR(queue_family, surface->_data->surface))
        GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
    vsl::Swapchain first(device, surface, std::nullopt, 64, 48);
    BorrowedImageViews first_views{first}; // I23: production destructor omits borrowed views.
    ASSERT_GT(first.getSwapImageSize(), 0u);
    EXPECT_EQ(first.getSwapImageSize(), first.images.size());
    int framebuffer_width = 0, framebuffer_height = 0;
    glfwGetFramebufferSize(static_cast<GLFWwindow *>(window->_data->window_handle), &framebuffer_width,
                           &framebuffer_height);
    EXPECT_EQ(first._data->swapChainExtent.width, static_cast<unsigned>(framebuffer_width));
    EXPECT_EQ(first._data->swapChainExtent.height, static_cast<unsigned>(framebuffer_height));
    for (const auto &image : first.images)
        EXPECT_TRUE(static_cast<bool>(image._data->view));
    device._data->device.waitIdle();
    vsl::Swapchain recreated(device, surface, first, 64, 48);
    BorrowedImageViews recreated_views{recreated};
    EXPECT_TRUE(static_cast<bool>(recreated._data->swapChain));
    EXPECT_EQ(recreated.getSwapImageSize(), recreated.images.size());
}
} // namespace
