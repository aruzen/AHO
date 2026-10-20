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

namespace {
    TEST(SurfaceDataTest, EmptySurfaceDataCanBeDestroyedWithoutCallingVulkan) {
        auto empty = std::make_shared<vsl::_impl::Surface_impl>();
        EXPECT_NO_THROW(empty.reset());
    }
    TEST_F(SurfaceWindowIntegrationTest, NullWindowIsRejectedBeforeNativeAccess) {
        EXPECT_THROW((vsl::Surface(nullptr, vsl::VulkanAccessor{instance})), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, EmptyWindowDataIsRejectedBeforeGlfwAccess) {
        vsl::PureWindow empty(std::shared_ptr<vsl::PureWindow::WindowData>{});
        EXPECT_THROW((vsl::Surface(&empty, vsl::VulkanAccessor{instance})), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, ClosedWindowIsRejectedBeforeGlfwAccess) {
        ASSERT_TRUE(window->close());
        EXPECT_THROW((vsl::Surface(&*window, vsl::VulkanAccessor{instance})), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, EmptyInstanceIsRejectedBeforeNativeAccess) {
        EXPECT_THROW((vsl::Surface(&*window, vsl::VulkanAccessor{})), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, NullNativeInstanceIsRejectedBeforeGlfwAccess) {
        auto empty = std::make_shared<vsl::_impl::Vulkan_impl<false>>();
        EXPECT_THROW((vsl::Surface(&*window, vsl::VulkanAccessor{empty})), vsl::exceptions::RuntimeException);
    }
    TEST_F(SurfaceWindowIntegrationTest, MissingPlatformExtensionThrowsWithReasonAndDoesNotRegisterAPlugin) {
        auto unsupported = std::make_shared<vsl::_impl::Vulkan_impl<false>>();
        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
        vk::InstanceCreateInfo info{};
#ifdef __APPLE__
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        info.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif
        info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        unsupported->instance = vk::createInstance(info);
        const auto previousCount = window->_data->plugins.size();
        const auto previousId = window->_data->plugins_next_id;
        try {
            window->add_plugin<vsl::Surface>(vsl::VulkanAccessor{unsupported});
            ADD_FAILURE() << "Failed surface creation returned a registered plugin";
        } catch (const vsl::exceptions::RuntimeException& error) {
            const std::string reason = error.what();
            EXPECT_NE(reason.find("Surface"), std::string::npos);
            EXPECT_NE(reason.find("VkResult"), std::string::npos);
            EXPECT_NE(reason.find(std::to_string(VK_ERROR_EXTENSION_NOT_PRESENT)), std::string::npos);
            EXPECT_NE(reason.find("GLFW"), std::string::npos);
        }
        EXPECT_EQ(window->_data->plugins.size(), previousCount);
        EXPECT_EQ(window->_data->plugins_next_id, previousId);
        make_surface();
        ASSERT_FALSE(HasFatalFailure());
        EXPECT_TRUE(static_cast<bool>(surface->_data->surface));
    }
} // namespace
