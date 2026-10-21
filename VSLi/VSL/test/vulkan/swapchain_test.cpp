#include "support/window_fixture.hpp"
#include "support/read_image.hpp"
namespace {
    class SwapchainWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(SwapchainWindowIntegrationTest, CreatesImagesAndRecreatesUsingOldSwapchain) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain first(device, surface, std::nullopt, 64, 48);
        ASSERT_GT(first.get_swap_image_size(), 0u);
        EXPECT_EQ(first.get_swap_image_size(), first.images.size());
        int framebuffer_width = 0, framebuffer_height = 0;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window->data_->window_handle), &framebuffer_width,
                               &framebuffer_height);
        EXPECT_EQ(first.data_->swap_chain_extent.width, static_cast<unsigned>(framebuffer_width));
        EXPECT_EQ(first.data_->swap_chain_extent.height, static_cast<unsigned>(framebuffer_height));
        for (const auto& image : first.images)
            EXPECT_TRUE(static_cast<bool>(image.data_->view));
        device.data_->device.waitIdle();
        vsl::Swapchain recreated(device, surface, first, 64, 48);
        EXPECT_TRUE(static_cast<bool>(recreated.data_->swap_chain));
        EXPECT_EQ(recreated.get_swap_image_size(), recreated.images.size());
    }
} // namespace

namespace {
    class SwapchainReadbackWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(SwapchainReadbackWindowIntegrationTest, ReadsAcquiredSwapchainImageAndRetainsBorrowedImageOwner) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        const auto caps = physical.data_->device.getSurfaceCapabilitiesKHR(surface->data_->surface);
        if (!(caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) ||
            !physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Surface cannot provide presentable transfer-source images on selected queue";
        auto chain = std::make_optional<vsl::Swapchain>(device, surface, std::nullopt, 64, 48,
                                                        vsl::ImageType::COLOR_ATTACHMENT | vsl::ImageType::TRANSFER_SRC);
        EXPECT_EQ(chain->get_format(), chain->images.front().get_format());
        auto acquired = device.data_->device.createFenceUnique({});
        const auto index = device.data_->device.acquireNextImageKHR(chain->data_->swap_chain, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device.data_->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        auto image = chain->images[index];
        const auto extent = chain->data_->extent;
        std::weak_ptr<vsl::impl::SwapchainImpl> owner = chain->data_;
        {
            vsl::RenderPassDescription desc;
            desc.add_color({.format = chain->get_format(), .final_layout = vsl::ImageLayout::GENERAL});
            vsl::RenderPass pass(device, desc);
            vsl::FrameBuffer target(pass, extent.width, extent.height, {image});
            auto manager = make_manager();
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                phase << vsl::command::RenderPassBegin(target, {vsl::ClearColor{1, 0, 0, 1}}) << vsl::command::RenderPassEnd{};
            }
            auto pixels = vsl_test::read_image(manager, image, vsl::ImageLayout::GENERAL);
            ASSERT_EQ(pixels.size(), std::size_t(extent.width) * extent.height * 4);
            EXPECT_EQ(image.get_format(), chain->get_format());
            const bool bgra = image.get_format().type() == vsl::data_format::FormatTypes::SRGB8_BGRA ||
                              image.get_format().type() == vsl::data_format::FormatTypes::UNSIGNED_NORMALIZED8_BGRA;
            EXPECT_EQ(pixels[bgra ? 2 : 0], std::byte{255});
            EXPECT_EQ(pixels[1], std::byte{0});
            chain.reset();
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_FALSE(owner.expired());
        image.data_.reset();
        EXPECT_TRUE(owner.expired());
    }
} // namespace

#include "../../src/vulkan/swapchain_sharing.hpp"
namespace {
    TEST(SwapchainSharingTest, ConcurrentImagesKeepEverySelectedSurfaceTransform) {
        const std::uint32_t families[]{2, 7};
        for (const auto transform : {vk::SurfaceTransformFlagBitsKHR::eIdentity, vk::SurfaceTransformFlagBitsKHR::eRotate90,
                                     vk::SurfaceTransformFlagBitsKHR::eRotate180, vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror}) {
            SCOPED_TRACE(vk::to_string(transform));
            vk::SwapchainCreateInfoKHR info{};
            vsl::impl::helper::configure_swapchain_sharing(info, families, transform);
            EXPECT_EQ(info.preTransform, transform);
            EXPECT_EQ(info.imageSharingMode, vk::SharingMode::eConcurrent);
            ASSERT_EQ(info.queueFamilyIndexCount, 2u);
            ASSERT_EQ(info.pQueueFamilyIndices, families);
            EXPECT_EQ(info.pQueueFamilyIndices[0], 2u);
            EXPECT_EQ(info.pQueueFamilyIndices[1], 7u);
        }
    }
    TEST(SwapchainSharingTest, ExclusiveImagesKeepTransformAndNeedNoFamilyList) {
        const std::uint32_t families[]{4, 4};
        vk::SwapchainCreateInfoKHR info{};
        vsl::impl::helper::configure_swapchain_sharing(info, families, vk::SurfaceTransformFlagBitsKHR::eRotate270);
        EXPECT_EQ(info.preTransform, vk::SurfaceTransformFlagBitsKHR::eRotate270);
        EXPECT_EQ(info.imageSharingMode, vk::SharingMode::eExclusive);
        EXPECT_EQ(info.queueFamilyIndexCount, 0u);
        EXPECT_EQ(info.pQueueFamilyIndices, nullptr);
    }
    TEST(SwapchainSharingTest, ReconfiguringForOneFamilyClearsThePreviousFamilyList) {
        const std::uint32_t different[]{1, 3}, same[]{1, 1};
        vk::SwapchainCreateInfoKHR info{};
        vsl::impl::helper::configure_swapchain_sharing(info, different, vk::SurfaceTransformFlagBitsKHR::eIdentity);
        vsl::impl::helper::configure_swapchain_sharing(info, same, vk::SurfaceTransformFlagBitsKHR::eRotate90);
        EXPECT_EQ(info.preTransform, vk::SurfaceTransformFlagBitsKHR::eRotate90);
        EXPECT_EQ(info.imageSharingMode, vk::SharingMode::eExclusive);
        EXPECT_EQ(info.queueFamilyIndexCount, 0u);
        EXPECT_EQ(info.pQueueFamilyIndices, nullptr);
    }
    TEST(SwapchainSharingTest, ReconfiguringForTwoFamiliesUpdatesTransformAndFamilyList) {
        const std::uint32_t same[]{1, 1}, different[]{1, 3};
        vk::SwapchainCreateInfoKHR info{};
        vsl::impl::helper::configure_swapchain_sharing(info, same, vk::SurfaceTransformFlagBitsKHR::eRotate90);
        vsl::impl::helper::configure_swapchain_sharing(info, different, vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror);
        EXPECT_EQ(info.preTransform, vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror);
        EXPECT_EQ(info.imageSharingMode, vk::SharingMode::eConcurrent);
        EXPECT_EQ(info.queueFamilyIndexCount, 2u);
        EXPECT_EQ(info.pQueueFamilyIndices, different);
    }
} // namespace

namespace {
    TEST_F(SwapchainWindowIntegrationTest, NativeCreationFailureIsReportedBeforeQueryingImagesAndAllowsValidRetry) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain first(device, surface, std::nullopt, 64, 48);
        vsl::Swapchain current(device, surface, first, 64, 48);
        const auto active = current.data_->swap_chain;
        // A retired oldSwapchain is rejected by the validation layer, which aborts before the driver call.
        expect_validation_abort("VUID-VkSwapchainCreateInfoKHR-oldSwapchain-01933");
        try {
            vsl::Swapchain rejected(device, surface, first, 64, 48);
            ADD_FAILURE() << "vkCreateSwapchainKHR failure was accepted";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find("vkCreateSwapchainKHR"), std::string::npos);
            EXPECT_NE(error.reason.find("Validation"), std::string::npos);
        }
        EXPECT_EQ(validation_abort_count(), 1u);
        EXPECT_EQ(current.data_->swap_chain, active);
        auto acquired = device.data_->device.createFenceUnique({});
        const auto index = device.data_->device.acquireNextImageKHR(active, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device.data_->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        EXPECT_LT(index, current.images.size());
        vsl::Swapchain retry(device, surface, current, 64, 48);
        EXPECT_TRUE(static_cast<bool>(retry.data_->swap_chain));
        EXPECT_FALSE(retry.images.empty());
    }
} // namespace

namespace {
    TEST(SwapchainCompositeAlphaTest, OpaqueRemainsPreferredWhenSupported) {
        const auto supported = vk::CompositeAlphaFlagBitsKHR::eOpaque | vk::CompositeAlphaFlagBitsKHR::ePreMultiplied |
                               vk::CompositeAlphaFlagBitsKHR::ePostMultiplied | vk::CompositeAlphaFlagBitsKHR::eInherit;
        EXPECT_EQ(vsl::impl::helper::choose_swapchain_composite_alpha(supported), vk::CompositeAlphaFlagBitsKHR::eOpaque);
    }
    TEST(SwapchainCompositeAlphaTest, EverySingleSupportedModeIsSelected) {
        for (const auto mode : {vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                                vk::CompositeAlphaFlagBitsKHR::ePostMultiplied, vk::CompositeAlphaFlagBitsKHR::eInherit}) {
            SCOPED_TRACE(vk::to_string(mode));
            EXPECT_EQ(vsl::impl::helper::choose_swapchain_composite_alpha(mode), mode);
        }
    }
    TEST(SwapchainCompositeAlphaTest, CombinedFallbackModesNeverSelectAnUnsupportedFlag) {
        for (const auto supported : {
                 vk::CompositeAlphaFlagBitsKHR::ePreMultiplied | vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
                 vk::CompositeAlphaFlagBitsKHR::ePostMultiplied | vk::CompositeAlphaFlagBitsKHR::eInherit,
                 vk::CompositeAlphaFlagBitsKHR::ePreMultiplied | vk::CompositeAlphaFlagBitsKHR::eInherit}) {
            SCOPED_TRACE(vk::to_string(supported));
            const auto selected = vsl::impl::helper::choose_swapchain_composite_alpha(supported);
            EXPECT_TRUE(static_cast<bool>(supported & selected));
            EXPECT_NE(selected, vk::CompositeAlphaFlagBitsKHR::eOpaque);
        }
    }
    TEST(SwapchainCompositeAlphaTest, EmptySupportIsRejectedBeforeNativeCreation) {
        EXPECT_THROW(vsl::impl::helper::choose_swapchain_composite_alpha({}), vsl::exceptions::RuntimeException);
    }
    TEST_F(SwapchainWindowIntegrationTest, NativeSurfaceSupportsTheSelectedCompositeAlpha) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        const auto caps = physical.data_->device.getSurfaceCapabilitiesKHR(surface->data_->surface);
        const auto selected = vsl::impl::helper::choose_swapchain_composite_alpha(caps.supportedCompositeAlpha);
        ASSERT_TRUE(static_cast<bool>(caps.supportedCompositeAlpha & selected));
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain chain(device, surface, std::nullopt, 64, 48);
        EXPECT_TRUE(static_cast<bool>(chain.data_->swap_chain));
        EXPECT_FALSE(chain.images.empty());
    }
} // namespace
