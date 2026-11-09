#include <VSL/lifetime/swapchain_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include "support/window_fixture.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/dependency_node.hpp>
#include "support/read_image.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::SwapchainAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
}
namespace {
    class SwapchainWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(SwapchainWindowIntegrationTest, EmptyOldAccessorIsRejectedBeforeNativeCreation) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_THROW((vsl::Swapchain(device, surface, vsl::SwapchainAccessor{}, 64, 48)), std::invalid_argument);
    }

    TEST_F(SwapchainWindowIntegrationTest, OldSwapchainFromAnotherSurfaceIsRejectedWithoutRetiringIt) {
        expect_validation_abort("");
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain original(device, surface, std::nullopt, 64, 48);
        vsl::PureWindow other("other swapchain surface", 64, 48);
        struct CloseWindow {
            vsl::PureWindow& window;
            ~CloseWindow() {
                window.data_->plugins.clear();
                window.close();
            }
        } close{other};
        auto other_surface = other.add_plugin<vsl::Surface>(vsl::VulkanAccessor{instance});
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, other_surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to the second surface";
        EXPECT_THROW((vsl::Swapchain(device, other_surface, original, 64, 48)), std::invalid_argument);
        auto acquired = device.data_->device.createFenceUnique({});
        const auto index = device.data_->device.acquireNextImageKHR(SwapchainAccess::Owned(original)->swap_chain, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device.data_->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        EXPECT_LT(index, original.get_swap_image_size());
    }

    TEST_F(SwapchainWindowIntegrationTest, CreatesImagesAndRecreatesUsingOldSwapchain) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain first(device, surface, std::nullopt, 64, 48);
        ASSERT_GT(first.get_swap_image_size(), 0u);
        EXPECT_EQ(first.get_swap_image_size(), first.images().size());
        int framebuffer_width = 0, framebuffer_height = 0;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window->data_->window_handle), &framebuffer_width,
                               &framebuffer_height);
        EXPECT_EQ(SwapchainAccess::Owned(first)->swap_chain_extent.width, static_cast<unsigned>(framebuffer_width));
        EXPECT_EQ(SwapchainAccess::Owned(first)->swap_chain_extent.height, static_cast<unsigned>(framebuffer_height));
        for (const auto& image : first.images())
            EXPECT_TRUE(static_cast<bool>(ImageAccess::Owned(image)->view));
        device.data_->device.waitIdle();
        vsl::Swapchain recreated(device, surface, first, 64, 48);
        EXPECT_TRUE(static_cast<bool>(SwapchainAccess::Owned(recreated)->swap_chain));
        EXPECT_EQ(recreated.get_swap_image_size(), recreated.images().size());
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
        EXPECT_EQ(chain->get_format(), chain->images().front().get_format());
        auto acquired = device.data_->device.createFenceUnique({});
        const auto index = device.data_->device.acquireNextImageKHR(SwapchainAccess::Owned(*(chain))->swap_chain, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device.data_->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        auto image = chain->images()[index];
        const auto extent = SwapchainAccess::Owned(*(chain))->extent;
        std::weak_ptr<vsl::impl::SwapchainImpl> owner = SwapchainAccess::Owned(*(chain)).share();
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
        ImageAccess::Owned(image).reset();
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
    TEST_F(SwapchainWindowIntegrationTest, RetiredOldSwapchainIsRejectedBeforeNativeCreationAndLeavesCurrentUsable) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        if (!physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected graphics queue cannot present to this surface";
        vsl::Swapchain first(device, surface, std::nullopt, 64, 48);
        vsl::Swapchain current(device, surface, first, 64, 48);
        const auto active = SwapchainAccess::Owned(current)->swap_chain;
        EXPECT_EQ(first.state(), vsl::SwapchainState::RETIRED);
        EXPECT_THROW((vsl::Swapchain(device, surface, first, 64, 48)), std::logic_error);
        EXPECT_EQ(validation_abort_count(), 0u);
        EXPECT_EQ(SwapchainAccess::Owned(current)->swap_chain, active);
        auto acquired = device.data_->device.createFenceUnique({});
        const auto index = device.data_->device.acquireNextImageKHR(active, UINT64_MAX, nullptr, *acquired).value;
        ASSERT_EQ(device.data_->device.waitForFences(*acquired, true, UINT64_MAX), vk::Result::eSuccess);
        EXPECT_LT(index, current.get_swap_image_size());
        vsl::Swapchain retry(device, surface, current, 64, 48);
        EXPECT_TRUE(static_cast<bool>(SwapchainAccess::Owned(retry)->swap_chain));
        EXPECT_FALSE(retry.images().empty());
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
        EXPECT_TRUE(static_cast<bool>(SwapchainAccess::Owned(chain)->swap_chain));
        EXPECT_FALSE(chain.images().empty());
    }
} // namespace

namespace swapchain_value_test {
    struct Metadata {
        vsl::lifetime::DependencyNode node;
        std::shared_ptr<void> release;
        vsl::impl::SwapchainImpl value;
        ~Metadata() { node.retire(); }
    };
    vsl::SwapchainAccessor make_metadata(size_t images, std::shared_ptr<void> release = {});
    vsl::SwapchainAccessor make_metadata(size_t images, std::shared_ptr<void> release) {
        auto owner = std::make_shared<Metadata>();
        owner->release = std::move(release);
        owner->value.swap_chain_image_format = vk::Format::eR8G8B8A8Unorm;
        owner->value.extent = owner->value.swap_chain_extent = vk::Extent2D{1, 1};
        owner->value.image_count = static_cast<std::uint32_t>(images);
        owner->node.publish(owner);
        owner->value.owner_node = &owner->node;
        std::vector<vsl::ImageAccessor> values;
        values.reserve(images);
        for (size_t i = 0; i < images; ++i)
            values.push_back(ImageAccess::FromShared(std::make_shared<vsl::impl::ImageImpl>()));
        auto* value = &owner->value;
        return SwapchainAccess::FromOwned(owner->node.ref(owner).alias(value), std::move(values));
    }
    TEST(SwapchainValueTest, AssignmentPublishesTheOwnerAndEveryImageBeforeReentrantRelease) {
        for (const bool destroy : {false, true}) {
            SCOPED_TRACE(destroy);
            const auto replacement = make_metadata(2), third = make_metadata(3);
            std::optional<vsl::SwapchainAccessor> target;
            bool invoked = false;
            const vsl_test::DeferredAction release([&] {
                invoked = true;
                ASSERT_TRUE(target);
                EXPECT_EQ(target->identity(), replacement.identity());
                ASSERT_EQ(target->get_swap_image_size(), 2u);
                for (size_t i = 0; i < 2; ++i)
                    EXPECT_EQ(ImageAccess::Owned(target->image(i)), ImageAccess::Owned(replacement.image(i)));
                if (destroy)
                    target.reset();
                else
                    *target = third;
            });
            target.emplace(make_metadata(1, release.owner()));
            *target = replacement;
            EXPECT_TRUE(invoked);
            if (destroy)
                EXPECT_FALSE(target);
            else {
                ASSERT_TRUE(target);
                EXPECT_EQ(target->identity(), third.identity());
                EXPECT_EQ(target->get_swap_image_size(), 3u);
            }
        }
    }
    TEST(SwapchainValueTest, CopiesMovesAndOwningImageValuesPreserveTheirIndependentLifetimes) {
        auto original = make_metadata(2);
        auto copied = original;
        auto moved = std::move(copied);
        EXPECT_FALSE(copied);
        EXPECT_EQ(copied.identity(), vsl::SwapchainIdentity{});
        EXPECT_EQ(copied.get_swap_image_size(), 0u);
        EXPECT_TRUE(copied.images().empty());
        EXPECT_EQ(moved.identity(), original.identity());
        EXPECT_EQ(moved.get_swap_image_size(), 2u);
        copied.swap(moved);
        EXPECT_FALSE(moved);
        EXPECT_EQ(copied.identity(), original.identity());
        EXPECT_EQ(copied.get_format(), vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        const std::weak_ptr<vsl::impl::ImageImpl> image_owner = ImageAccess::Owned(copied.image(0)).share();
        auto kept = std::move(copied).image(0);
        original = {};
        copied = {};
        EXPECT_FALSE(image_owner.expired());
        kept = {};
        EXPECT_TRUE(image_owner.expired());
        EXPECT_THROW(moved.image(0), std::out_of_range);
    }
    TEST(SwapchainValueTest, RetiredMetadataRejectsOwnerPublicationWithoutPreventingExistingValueCleanup) {
        auto value = make_metadata(1);
        const auto owner = SwapchainAccess::Owned(value);
        owner->owner_node->retire();
        EXPECT_THROW(value.owner_ref(), std::invalid_argument);
        EXPECT_THROW(value.require_image_usage(vsl::ImageType::COLOR_ATTACHMENT), std::invalid_argument);
        auto copied = value;
        value = {};
        EXPECT_TRUE(copied);
        EXPECT_NO_THROW(copied.image(0));
        EXPECT_NO_THROW(copied = {});
    }
} // namespace swapchain_value_test
