#include "support/window_fixture.hpp"
#include "support/read_image.hpp"
namespace {
    using SwapchainWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(SwapchainWindowIntegrationTest, CreatesImagesAndRecreatesUsingOldSwapchain) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical._data->device.getSurfaceSupportKHR(queue_family, surface->_data->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain first(device, surface, std::nullopt, 64, 48);
        ASSERT_GT(first.getSwapImageSize(), 0u);
        EXPECT_EQ(first.getSwapImageSize(), first.images.size());
        int framebuffer_width = 0, framebuffer_height = 0;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window->_data->window_handle), &framebuffer_width,
                               &framebuffer_height);
        EXPECT_EQ(first._data->swapChainExtent.width, static_cast<unsigned>(framebuffer_width));
        EXPECT_EQ(first._data->swapChainExtent.height, static_cast<unsigned>(framebuffer_height));
        for (const auto& image : first.images)
            EXPECT_TRUE(static_cast<bool>(image._data->view));
        device._data->device.waitIdle();
        vsl::Swapchain recreated(device, surface, first, 64, 48);
        EXPECT_TRUE(static_cast<bool>(recreated._data->swapChain));
        EXPECT_EQ(recreated.getSwapImageSize(), recreated.images.size());
    }
} // namespace

namespace {
    class SwapchainReadbackWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(SwapchainReadbackWindowIntegrationTest, ReadsAcquiredSwapchainImageAndRetainsBorrowedImageOwner) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        const auto caps = physical._data->device.getSurfaceCapabilitiesKHR(surface->_data->surface);
        if (!(caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) ||
            !physical._data->device.getSurfaceSupportKHR(queue_family, surface->_data->surface))
            GTEST_SKIP() << "Surface cannot provide presentable transfer-source images on selected queue";
        auto chain = std::make_optional<vsl::Swapchain>(device, surface, std::nullopt, 64, 48,
                                                        vsl::ImageType::ColorAttachment | vsl::ImageType::TransferSrc);
        EXPECT_EQ(chain->getFormat(), chain->images.front().getFormat());
        auto acquired = device._data->device.createFenceUnique({});
        const auto index = device._data->device.acquireNextImageKHR(chain->_data->swapChain, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device._data->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        auto image = chain->images[index];
        const auto extent = chain->_data->extent;
        std::weak_ptr<vsl::_impl::Swapchain_impl> owner = chain->_data;
        {
            vsl::RenderPassDescription desc;
            desc.addColor({.format = chain->getFormat(), .finalLayout = vsl::ImageLayout::General});
            vsl::RenderPass pass(device, desc);
            vsl::FrameBuffer target(pass, extent.width, extent.height, {image});
            auto manager = make_manager();
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                phase << vsl::command::RenderPassBegin(target, {vsl::ClearColor{1, 0, 0, 1}}) << vsl::command::RenderPassEnd{};
            }
            auto pixels = vsl_test::read_image(manager, image, vsl::ImageLayout::General);
            ASSERT_EQ(pixels.size(), std::size_t(extent.width) * extent.height * 4);
            EXPECT_EQ(image.getFormat(), chain->getFormat());
            const bool bgra = image.getFormat().type() == vsl::data_format::___FormatTypes::Srgb8BGRA ||
                              image.getFormat().type() == vsl::data_format::___FormatTypes::UnsignedNormalized8BGRA;
            EXPECT_EQ(pixels[bgra ? 2 : 0], std::byte{255});
            EXPECT_EQ(pixels[1], std::byte{0});
            chain.reset();
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_FALSE(owner.expired());
        image._data.reset();
        EXPECT_TRUE(owner.expired());
    }
} // namespace
