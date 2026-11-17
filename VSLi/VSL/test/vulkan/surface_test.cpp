#include <VSL/lifetime/startup_access.hpp>
#include "support/window_fixture.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}
namespace {
    using SurfaceWindowIntegrationTest = vsl_test::WindowIntegrationTest;
    TEST_F(SurfaceWindowIntegrationTest, CreatesSurfaceForWindowAndRetainsInstance) {
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_EQ(StartupAccess::Owned(*(surface))->vulkan, instance.get());
        EXPECT_EQ(StartupAccess::Owned(surface->get_instance()), instance);
        EXPECT_EQ(StartupAccess::Owned(*(surface))->window, window->data_->window_handle);
        const auto capabilities = StartupAccess::Owned(physical)->device.getSurfaceCapabilitiesKHR(StartupAccess::Owned(*(surface))->surface);
        EXPECT_GE(capabilities.maxImageExtent.width, 64u);
        EXPECT_GE(capabilities.maxImageExtent.height, 48u);
        EXPECT_FALSE(StartupAccess::Owned(physical)->device.getSurfaceFormatsKHR(StartupAccess::Owned(*(surface))->surface).empty());
    }
    TEST_F(SurfaceWindowIntegrationTest, OwnerAssignmentPreservesTheDestinationPluginRegistration) {
        make_surface();
        auto peer = window->add_plugin<vsl::Surface>(StartupAccess::FromShared(instance));
        const auto id = surface->id, peer_id = peer->id;
        ASSERT_NE(id, peer_id);
        const auto source = StartupAccess::Owned(*peer);
        *surface = *peer;
        EXPECT_EQ(surface->id, id);
        EXPECT_EQ(peer->id, peer_id);
        EXPECT_EQ(StartupAccess::Owned(*surface), source);
        EXPECT_EQ(window->data_->plugins.at(id).get(), surface.get());
        EXPECT_TRUE(window->remove_plugin(surface));
        EXPECT_FALSE(window->data_->plugins.contains(id));
        EXPECT_EQ(window->data_->plugins.at(peer_id).get(), peer.get());
    }
} // namespace

namespace {
    TEST(SurfaceDataTest, EmptySurfaceDataCanBeDestroyedWithoutCallingVulkan) {
        auto empty = std::make_shared<vsl::impl::SurfaceImpl>();
        EXPECT_NO_THROW(empty.reset());
    }
    TEST_F(SurfaceWindowIntegrationTest, NullWindowIsRejectedBeforeNativeAccess) {
        EXPECT_THROW((vsl::Surface(nullptr, vsl::lifetime::detail::StartupAccess::FromShared(instance))), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, EmptyWindowDataIsRejectedBeforeGlfwAccess) {
        vsl::PureWindow empty(std::shared_ptr<vsl::PureWindow::WindowData>{});
        EXPECT_THROW((vsl::Surface(&empty, vsl::lifetime::detail::StartupAccess::FromShared(instance))), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, ClosedWindowIsRejectedBeforeGlfwAccess) {
        ASSERT_TRUE(window->close());
        EXPECT_THROW((vsl::Surface(&*window, vsl::lifetime::detail::StartupAccess::FromShared(instance))), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, EmptyInstanceIsRejectedBeforeNativeAccess) {
        EXPECT_THROW((vsl::Surface(&*window, vsl::VulkanAccessor{})), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, NullNativeInstanceIsRejectedBeforeGlfwAccess) {
        auto empty = std::make_shared<vsl::impl::VulkanImpl<false>>();
        EXPECT_THROW((vsl::Surface(&*window, vsl_test::unchecked_startup(empty))), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, MissingPlatformExtensionThrowsWithReasonAndDoesNotRegisterAPlugin) {
        vsl::InstanceCreateInfo info;
        info.extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __APPLE__
        info.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        const auto unsupported = vsl::VulkanAccessor::Create(std::move(info));
        const auto previous_count = window->data_->plugins.size();
        const auto previous_id = window->data_->plugins_next_id;
        try {
            window->add_plugin<vsl::Surface>(unsupported);
            ADD_FAILURE() << "Failed surface creation returned a registered plugin";
        } catch (const vsl::exceptions::RuntimeException& error) {
            const std::string reason = error.what();
            EXPECT_NE(reason.find("Surface"), std::string::npos);
            EXPECT_NE(reason.find("VkResult"), std::string::npos);
            EXPECT_NE(reason.find(std::to_string(VK_ERROR_EXTENSION_NOT_PRESENT)), std::string::npos);
            EXPECT_NE(reason.find("GLFW"), std::string::npos);
        }
        EXPECT_EQ(window->data_->plugins.size(), previous_count);
        EXPECT_EQ(window->data_->plugins_next_id, previous_id);
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_TRUE(static_cast<bool>(StartupAccess::Owned(*(surface))->surface));
    }
} // namespace
