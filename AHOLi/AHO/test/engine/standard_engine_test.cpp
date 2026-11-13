#include <VSL/lifetime/startup_access.hpp>
#include <limits>
#include <algorithm>
#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include <VSL/lifetime/swapchain_arena.hpp>
namespace {
    using StandardEngineVulkanIntegration = aho_test::LiveGPUTest;
    TEST_F(StandardEngineVulkanIntegration, ConstructorCreatesDeviceWindowAndFrameResources) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_NE(engine->data(), nullptr);
        ASSERT_NE(engine->data()->logical_device.data_, nullptr);
        EXPECT_FALSE(engine->data()->physical_device.name().empty());
        ASSERT_TRUE(engine->boot_window.has_value());
        auto window = engine->get_window();
        ASSERT_NE(window.data_->window_handle, nullptr);
        ASSERT_NE(window.data2, nullptr);
        EXPECT_EQ(window.window_size().value.x.value, 400);
        EXPECT_EQ(window.window_size().value.y.value, 400);
        EXPECT_GT(window.frame_size().value.x.value, 0);
        EXPECT_GT(window.frame_size().value.y.value, 0);
        EXPECT_NE(window.data2->surface, nullptr);
    }
    TEST_F(StandardEngineVulkanIntegration, StartupUsesIndependentInstanceDeviceAndSurfaceOwners) {
        ASSERT_NO_THROW(start_engine());
        const auto& data = *engine->data();
        const auto& surface = engine->boot_window->data2->surface;
        EXPECT_FALSE(data.logical_device.data_.owner_before(VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(data.physical_device)));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.physical_device).owner_before(data.logical_device.data_));
        EXPECT_TRUE(data.logical_device.data_.owner_before(VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(data.vulkan_instance)) ||
                    VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.vulkan_instance).owner_before(data.logical_device.data_));
        EXPECT_TRUE(surface->data_.owner_before(data.logical_device.data_) || data.logical_device.data_.owner_before(surface->data_));
        EXPECT_TRUE(surface->data_.owner_before(VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(data.vulkan_instance)) || VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.vulkan_instance).owner_before(surface->data_));
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.logical_device.get_physical_device()), VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.physical_device));
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.physical_device.get_instance()), VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.vulkan_instance));
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(surface->get_instance()), VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(data.vulkan_instance));
    }
    TEST_F(StandardEngineVulkanIntegration, SavedDeviceDoesNotKeepClosedSurfaceOrWindowAlive) {
        ASSERT_NO_THROW(start_engine());
        auto saved = engine->data()->logical_device;
        const std::weak_ptr<vsl::impl::SurfaceImpl> surface = engine->boot_window->data2->surface->data_;
        const auto os = engine->boot_window->window_owner().owner_ref().weak();
        const std::weak_ptr<vsl::impl::VulkanImplAccessor> instance = VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(engine->data()->vulkan_instance);
        EXPECT_TRUE(engine->boot_window->close());
        engine.reset();
        EXPECT_TRUE(surface.expired());
        EXPECT_TRUE(os.expired());
        EXPECT_FALSE(instance.expired());
        EXPECT_TRUE(saved.get_physical_device().owner_ref().managed());
        saved.data_.reset();
        EXPECT_TRUE(instance.expired());
    }
    TEST_F(StandardEngineVulkanIntegration, ClosingTheInitialWindowLeavesAnIndependentSurfaceUsable) {
        ASSERT_NO_THROW(start_engine());
        auto& first = *engine->boot_window;
        aho::Window second("independent startup Surface", 64, 48, *engine);
        const std::weak_ptr<vsl::impl::SurfaceImpl> previous = first.data2->surface->data_;
        EXPECT_NE(second.data2->surface->data_, first.data2->surface->data_);
        EXPECT_TRUE(first.close());
        EXPECT_TRUE(previous.expired());
        EXPECT_TRUE(static_cast<bool>(second));
        EXPECT_TRUE(second.resize(80, 64));
        EXPECT_TRUE(second.close());
    }
    TEST_F(StandardEngineVulkanIntegration, ExplicitPresentFencePolicyEnablesInstanceAndDeviceTogether) {
        const auto available = vk::enumerateInstanceExtensionProperties();
        if (!std::ranges::any_of(available, [](const auto& extension) {
                return std::string_view(extension.extensionName.data()) == VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
            }))
            GTEST_SKIP() << "Surface maintenance Instance extension unavailable";
        vsl::InstanceCreateInfo info;
#ifdef __APPLE__
        info.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        const auto probe = vsl::VulkanAccessor::Create(vsl::lifetime::glfw_instance_info(std::move(info), vsl::PresentFencePolicy::REQUIRE_FENCE));
        const auto physical = vsl::PhysicalDevice::Select(probe);
        if (!vsl::has_device_features(physical.supported_features(), vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1))
            GTEST_SKIP() << "Swapchain maintenance feature unavailable";
        aho::engine::StandardEngineConfig config;
        config.engine.present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE;
        engine = std::make_unique<aho::StandardEngine>(config);
        EXPECT_TRUE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(engine->data()->vulkan_instance)->surface_capabilities2);
        EXPECT_TRUE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(engine->data()->vulkan_instance)->surface_maintenance1);
        EXPECT_TRUE(vsl::has_device_features(engine->data()->logical_device.enabled_features(), vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1));
        EXPECT_TRUE(static_cast<bool>(engine->get_window()));
        auto& window = *engine->boot_window;
        EXPECT_EQ(engine->data()->present_fences, vsl::PresentFencePolicy::REQUIRE_FENCE);
        EXPECT_EQ(window.data2->swapchain.present_fences(), vsl::PresentFencePolicy::REQUIRE_FENCE);
        aho::Window second("strict secondary", 64, 48, *engine);
        EXPECT_EQ(second.data2->swapchain.present_fences(), vsl::PresentFencePolicy::REQUIRE_FENCE);
        aho::Window legacy("explicit legacy", 64, 48, *engine, vsl::PresentFencePolicy::LEGACY);
        EXPECT_EQ(legacy.data2->swapchain.present_fences(), vsl::PresentFencePolicy::LEGACY);
        ASSERT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(window.data2->swapchain.present_fences(), vsl::PresentFencePolicy::REQUIRE_FENCE);
        EXPECT_TRUE(second.close());
        EXPECT_TRUE(legacy.close());
    }
    TEST_F(StandardEngineVulkanIntegration, BasePointerDestructionReleasesClosedWindowAndEngineResources) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_TRUE(engine->boot_window.has_value());
        const std::weak_ptr<aho::engine::EngineData> engine_data = engine->data();
        const std::weak_ptr<vsl::PureWindow::WindowData> window_data = engine->boot_window->data_;
        engine->data()->logical_device.data_->device.waitIdle();
        ASSERT_TRUE(engine->boot_window->close());
        std::unique_ptr<aho::engine::GraphicalEngine> base = std::move(engine);
        EXPECT_FALSE(engine_data.expired());
        EXPECT_FALSE(window_data.expired());
        base.reset();
        EXPECT_TRUE(engine_data.expired());
        EXPECT_TRUE(window_data.expired());
    }
    TEST_F(StandardEngineVulkanIntegration, ExplicitBootInitializesPreviouslyDeferredEngine) {
        ASSERT_NO_THROW(start_engine(false));
        EXPECT_EQ(engine->data(), nullptr);
        ASSERT_NO_THROW(engine->boot("AHO deferred boot"));
        ASSERT_TRUE(engine->boot_window.has_value());
        EXPECT_EQ(engine->get_window().name(), "AHO deferred boot");
        EXPECT_EQ(engine->get_window().engine.data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, MovedRegistrationAndHandleKeepTheDefaultAndRestoreThePreviousOne) {
        ASSERT_NO_THROW(start_engine());
        auto alternate = std::make_shared<aho::engine::EngineData>(*engine->data());
        {
            aho::engine::EngineAccessor handle(alternate);
            aho::engine::DefaultEngineRegistration registration(handle);
            auto moved_handle = std::move(handle);
            auto moved_registration = std::move(registration);
            EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), alternate);
            EXPECT_EQ(moved_handle.data(), alternate);
            EXPECT_FALSE(handle.data());
        }
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, RemovingAnOlderRegistrationKeepsTheNewestRegistration) {
        ASSERT_NO_THROW(start_engine());
        std::optional<aho::engine::DefaultEngineRegistration> older(std::in_place, *engine);
        const auto alternate = std::make_shared<aho::engine::EngineData>(*engine->data());
        std::optional<aho::engine::DefaultEngineRegistration> newer(std::in_place, aho::engine::EngineAccessor(alternate));
        older.reset();
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), alternate);
        newer.reset();
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, DefaultRegistrationRetainsItsDataUntilTheRegistrationEnds) {
        ASSERT_NO_THROW(start_engine());
        std::weak_ptr<aho::engine::EngineData> data;
        std::optional<aho::engine::DefaultEngineRegistration> registration;
        {
            auto alternate = std::make_shared<aho::engine::EngineData>(*engine->data());
            data = alternate;
            registration.emplace(aho::engine::EngineAccessor(std::move(alternate)));
        }
        EXPECT_FALSE(data.expired());
        registration.reset();
        EXPECT_TRUE(data.expired());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, SharedHandleRetainsResourcesAfterTheOwnerUnregisters) {
        ASSERT_NO_THROW(start_engine());
        auto handle = aho::engine::EngineAccessor(*engine);
        const std::weak_ptr<aho::engine::EngineData> data = handle.data();
        engine->data()->command_manager.wait_idle();
        ASSERT_TRUE(engine->get_window().close());
        engine.reset();
        EXPECT_FALSE(aho::engine::DefaultEngineRegistration::Current().data());
        EXPECT_FALSE(data.expired());
        handle = {};
        EXPECT_TRUE(data.expired());
    }

    TEST_F(StandardEngineVulkanIntegration, ConfiguredBootUsesWindowAndDescriptorSettingsWithoutReplacingTheDefault) {
        ASSERT_NO_THROW(start_engine());
        aho::engine::StandardEngineConfig config;
        config.engine.application_name = "configured engine";
        config.engine.physical_device_index = 0;
        config.engine.descriptor_budget = {{vsl::graphic_resource::Type::UNIFORM_BUFFER, 7}};
        config.initial_window = {.title = "configured window", .width = 72, .height = 48};
        config.register_as_default = false;
        aho::StandardEngine configured(config);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
        auto window = configured.get_window();
        EXPECT_EQ(window.name(), "configured window");
        EXPECT_EQ(window.window_size().value.___AN1, 72);
        EXPECT_EQ(window.window_size().value.___AN2, 48);
        const auto manager = std::dynamic_pointer_cast<vsl::graphic_resource::OverAllocateManager>(configured.data()->graphic_resource_manager);
        ASSERT_NE(manager, nullptr);
        EXPECT_EQ(manager->size, config.engine.descriptor_budget);
        EXPECT_EQ(manager->other, 0u);
        EXPECT_TRUE(window.close());
    }

    TEST_F(StandardEngineVulkanIntegration, RepeatedBootPreservesTheExistingResourcesAndDefault) {
        ASSERT_NO_THROW(start_engine());
        const auto data = engine->data();
        const auto windows = vsl::PureWindow::WINDOWS;
        EXPECT_THROW(engine->boot(), std::logic_error);
        EXPECT_THROW(engine->boot("replacement"), std::logic_error);
        EXPECT_EQ(engine->data(), data);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), data);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
        EXPECT_EQ(engine->config().engine.application_name, "AHO integration test");
    }

    TEST_F(StandardEngineVulkanIntegration, FailedDeviceSelectionRemovesTheTemporaryWindowAndPreservesTheDefault) {
        ASSERT_NO_THROW(start_engine());
        aho::engine::StandardEngineConfig config;
        config.engine.physical_device_index = std::numeric_limits<std::size_t>::max();
        aho::StandardEngine failing(config, false);
        const auto windows = vsl::PureWindow::WINDOWS;
        EXPECT_THROW(failing.boot(), std::out_of_range);
        EXPECT_FALSE(failing.data());
        EXPECT_FALSE(failing.boot_window);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/engine/standard_engine.hpp>
namespace {
    TEST(StandardEngineTest, DeferredBootAllocatesNoGPUOrWindowAndGetWindowReportsAbsence) {
        const auto previous = aho::engine::DefaultEngineRegistration::Current().data();
        {
            aho::StandardEngine engine("deferred test", false);
            EXPECT_EQ(engine.data(), nullptr);
            EXPECT_FALSE(engine.boot_window.has_value());
            EXPECT_THROW(engine.get_window(), std::bad_optional_access);
        }
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), previous);
    }

    TEST(StandardEngineTest, DeferredConfigurationRetainsSettingsWithoutCreatingResources) {
        aho::engine::StandardEngineConfig config;
        config.engine.application_name = "deferred configuration";
        config.initial_window = {.title = "later", .width = 640, .height = 480};
        config.register_as_default = false;
        aho::StandardEngine engine(config, false);
        EXPECT_EQ(engine.config().engine.application_name, config.engine.application_name);
        EXPECT_EQ(engine.config().initial_window.width, 640);
        EXPECT_FALSE(engine.config().register_as_default);
        EXPECT_FALSE(engine.data());
        EXPECT_FALSE(engine.boot_window);
        EXPECT_THROW(engine.boot(""), std::invalid_argument);
        EXPECT_EQ(engine.config().engine.application_name, config.engine.application_name);
    }

    TEST(StandardEngineTest, InvalidConfigurationIsRejectedBeforeAllocatingAWindow) {
        const auto windows = vsl::PureWindow::WINDOWS;
        aho::engine::StandardEngineConfig config;
        config.initial_window.width = 0;
        EXPECT_THROW((aho::StandardEngine{config, false}), std::invalid_argument);
        config.initial_window.width = 400;
        config.engine.application_name.clear();
        EXPECT_THROW((aho::StandardEngine{config, false}), std::invalid_argument);
        config.engine.application_name = "invalid budget";
        config.engine.descriptor_budget.begin()->second = 0;
        EXPECT_THROW((aho::StandardEngine{config, false}), std::invalid_argument);
        config.engine.descriptor_budget = {{vsl::graphic_resource::Type::INLINE_UNIFORM_BLOCK, 1}};
        EXPECT_THROW((aho::StandardEngine{config, false}), std::invalid_argument);
        config.engine.descriptor_budget = {{vsl::graphic_resource::Type::UNIFORM_BUFFER, std::numeric_limits<std::uint32_t>::max()},
                                           {vsl::graphic_resource::Type::STORAGE_BUFFER, 1}};
        EXPECT_THROW((aho::StandardEngine{config, false}), std::invalid_argument);
        config.engine.descriptor_budget = {{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}};
        config.engine.present_fences = static_cast<vsl::PresentFencePolicy>(99);
        EXPECT_THROW((aho::StandardEngine{config, false}), std::invalid_argument);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
    }
} // namespace
#endif
