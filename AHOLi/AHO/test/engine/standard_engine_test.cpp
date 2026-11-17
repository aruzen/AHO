#include "../../src/engine/engine_access.hpp"
namespace {
    using AHO_NAMESPACE::engine::detail::EngineAccess;
}
#include <VSL/lifetime/startup_access.hpp>
#include <limits>
#include <algorithm>
#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/operation/swapchain_ops.hpp>
#include <functional>
namespace {
    using StandardEngineVulkanIntegration = aho_test::LiveGPUTest;
    struct BootNativeProbe {
        unsigned calls = 0;
        std::function<void()> callback;
        static VkResult Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* result);
    };
    struct BootCloseObserver : vsl::PureWindow::Plugin, vsl::PureWindow::OnUpdate {
        std::function<void()> callback;
        BootCloseObserver(vsl::PureWindow*, std::function<void()> callback) : callback(std::move(callback)) {}
        ~BootCloseObserver() override { callback(); }
        bool on_update(vsl::PureWindow) override { return true; }
    };
    VkResult BootNativeProbe::Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output) {
        auto& probe = *static_cast<BootNativeProbe*>(context);
        ++probe.calls;
        const auto result = vkCreateSwapchainKHR(device, info, nullptr, output);
        if (result == VK_SUCCESS) {
            const auto callback = std::exchange(probe.callback, {});
            if (callback)
                callback();
        }
        return result;
    }
    TEST_F(StandardEngineVulkanIntegration, ConstructorCreatesDeviceWindowAndFrameResources) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_NE(EngineAccess::State(*(engine)), nullptr);
        ASSERT_NE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(EngineAccess::State(*(engine))->logical_device), nullptr);
        EXPECT_FALSE(EngineAccess::State(*(engine))->logical_device.get_physical_device().name().empty());
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
        const auto runtime = engine->runtime();
        const auto device = runtime.device();
        const auto physical_device = runtime.physical_device();
        const auto instance = runtime.instance();
        const auto& surface = engine->boot_window->data2->surface;
        EXPECT_FALSE(device.owner_ref().owner_before(physical_device.owner_ref()));
        EXPECT_FALSE(physical_device.owner_ref().owner_before(device.owner_ref()));
        EXPECT_TRUE(device.owner_ref().owner_before(instance.owner_ref()) || instance.owner_ref().owner_before(device.owner_ref()));
        EXPECT_TRUE(surface->owner_ref().owner_before(device.owner_ref()) || device.owner_ref().owner_before(surface->owner_ref()));
        EXPECT_TRUE(surface->owner_ref().owner_before(instance.owner_ref()) || instance.owner_ref().owner_before(surface->owner_ref()));
        EXPECT_EQ(device.get_physical_device().owner_ref(), physical_device.owner_ref());
        EXPECT_EQ(physical_device.get_instance().owner_ref(), instance.owner_ref());
        EXPECT_EQ(surface->get_instance().owner_ref(), instance.owner_ref());
    }
    TEST_F(StandardEngineVulkanIntegration, PublicFactoryKeepsTheOriginalDeviceAndServices) {
        ASSERT_NO_THROW(start_engine());
        const auto original = engine->runtime();
        const auto created = aho::engine::EngineAccessor::Create({.device = original.device(), .commands = original.commands(), .descriptor_manager = original.descriptor_manager(), .synchronization = original.synchronization()});
        EXPECT_NE(created, original);
        EXPECT_EQ(created.device(), original.device());
        EXPECT_EQ(created.commands().identity(), original.commands().identity());
        EXPECT_EQ(created.descriptor_manager(), original.descriptor_manager());
        const auto actual = created.device().owner_ref(), expected = original.device().owner_ref();
        EXPECT_FALSE(actual.owner_before(expected));
        EXPECT_FALSE(expected.owner_before(actual));
        EXPECT_EQ(created.physical_device().owner_ref(), original.physical_device().owner_ref());
        EXPECT_EQ(created.instance().owner_ref(), original.instance().owner_ref());
        auto synchronization = created.synchronization();
        const auto fence = synchronization.create_fence("public factory registry");
        auto original_synchronization = original.synchronization();
        const auto found = original_synchronization.get_fence("public factory registry");
        ASSERT_TRUE(found);
        EXPECT_EQ(found->owner_ref(), fence.owner_ref());
    }
    TEST_F(StandardEngineVulkanIntegration, FactoryRejectsMissingOrForeignServicesWithoutChangingTheRuntimeOrDefault) {
        ASSERT_NO_THROW(start_engine());
        const auto original = engine->runtime();
        const auto previous = aho::engine::DefaultEngineRegistration::Current();
        const auto windows = vsl::PureWindow::WINDOWS;
        const auto other = vsl::LogicalDeviceAccessor::Create(original.physical_device());
        const auto good = aho::engine::EngineCreateInfo{
            .device = original.device(), .commands = original.commands(), .descriptor_manager = original.descriptor_manager(), .synchronization = original.synchronization()};
        for (unsigned field = 0; field < 8; ++field) {
            SCOPED_TRACE(field);
            auto info = good;
            switch (field) {
                case 0:
                    info.device = {};
                    break;
                case 1:
                    info.commands = {};
                    break;
                case 2:
                    info.descriptor_manager.reset();
                    break;
                case 3:
                    info.commands = vsl::CommandManager(other);
                    break;
                case 4:
                    info.descriptor_manager = std::make_shared<vsl::GraphicResourceManager>(other);
                    break;
                case 5:
                    info.synchronization = vsl::SynchroManager(other);
                    break;
                case 6:
                    info.present_fences = static_cast<vsl::PresentFencePolicy>(99);
                    break;
                case 7:
                    if (vsl::has_device_features(info.device.enabled_features(), vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1))
                        continue;
                    info.present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE;
                    break;
            }
            EXPECT_THROW(aho::engine::EngineAccessor::Create(std::move(info)), std::invalid_argument);
            EXPECT_EQ(engine->runtime(), original);
            EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current(), previous);
            EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
        }
    }
    TEST_F(StandardEngineVulkanIntegration, DestructionDuringNativeBootClosesTheCandidateAndRestoresTheDefault) {
        const auto windows = vsl::PureWindow::WINDOWS;
        const auto previous = aho::engine::DefaultEngineRegistration::Current();
        auto source = std::make_unique<aho::StandardEngine>("deleted during boot", false);
        BootNativeProbe probe;
        unsigned closes = 0;
        probe.callback = [&] {
            vsl::PureWindow candidate(vsl::PureWindow::WINDOWS.back());
            const std::weak_ptr<vsl::PureWindow::WindowData> native = candidate.data_;
            candidate.add_plugin<BootCloseObserver>([&, native] {
                ++closes;
                const auto state = native.lock();
                ASSERT_TRUE(state);
                EXPECT_FALSE(state->window_handle);
                EXPECT_TRUE(state->plugins.empty());
                EXPECT_TRUE(state->update.empty());
                EXPECT_EQ(std::ranges::find(vsl::PureWindow::WINDOWS, state), vsl::PureWindow::WINDOWS.end());
                EXPECT_FALSE(vsl::PureWindow(state).close());
            });
            source.reset();
        };
        const vsl::operation::ScopedSwapchainNativeCalls calls({&probe, BootNativeProbe::Create});
        EXPECT_THROW(source->boot(), std::logic_error);
        EXPECT_FALSE(source);
        EXPECT_EQ(probe.calls, 1u);
        EXPECT_EQ(closes, 1u);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current(), previous);
    }
    TEST_F(StandardEngineVulkanIntegration, PreviousWindowReleaseSeesTheCompleteRuntimeAndMayDestroyTheReceiver) {
        ASSERT_NO_THROW(start_engine());
        const auto previous = aho::engine::DefaultEngineRegistration::Current();
        const auto windows = vsl::PureWindow::WINDOWS;
        auto source = std::make_unique<aho::StandardEngine>("published before release", false);
        unsigned callbacks = 0;
        const vsl_test::DeferredAction action([&] {
            ++callbacks;
            ASSERT_TRUE(source);
            EXPECT_TRUE(source->runtime());
            ASSERT_TRUE(source->boot_window);
            EXPECT_TRUE(static_cast<bool>(source->get_window()));
            EXPECT_EQ(source->get_window().engine, source->runtime());
            EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current(), source->runtime());
            source.reset();
        });
        {
            aho::Window old("old boot window", 48, 32, engine->runtime());
            auto drawing = old.data2;
            ASSERT_TRUE(old.close());
            old.data2 = std::move(drawing);
            old.data2->standard_draw = {action.owner(), static_cast<aho::drawer::detail::StandardDrawState*>(nullptr)};
            source->boot_window.emplace(std::move(old));
        }
        EXPECT_THROW(source->boot(), std::logic_error);
        EXPECT_EQ(callbacks, 1u);
        EXPECT_FALSE(source);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current(), previous);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
    }
    TEST_F(StandardEngineVulkanIntegration, NativeCallbackCannotRecursivelyBootTheSameReceiver) {
        ASSERT_NO_THROW(start_engine(false));
        BootNativeProbe probe;
        probe.callback = [&] { EXPECT_THROW(engine->boot(), std::logic_error); };
        const vsl::operation::ScopedSwapchainNativeCalls calls({&probe, BootNativeProbe::Create});
        EXPECT_NO_THROW(engine->boot());
        EXPECT_EQ(probe.calls, 1u);
        EXPECT_TRUE(engine->runtime());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current(), engine->runtime());
        EXPECT_TRUE(static_cast<bool>(engine->get_window()));
    }
    TEST_F(StandardEngineVulkanIntegration, SavedDeviceDoesNotKeepClosedSurfaceOrWindowAlive) {
        ASSERT_NO_THROW(start_engine());
        auto saved = EngineAccess::State(*(engine))->logical_device;
        const std::weak_ptr<vsl::impl::SurfaceImpl> surface = VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(*(engine->boot_window->data2->surface));
        const auto os = engine->boot_window->window_owner().owner_ref().weak();
        const std::weak_ptr<vsl::impl::VulkanImplAccessor> instance = VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(EngineAccess::State(*(engine))->logical_device.get_physical_device().get_instance());
        EXPECT_TRUE(engine->boot_window->close());
        engine.reset();
        EXPECT_TRUE(surface.expired());
        EXPECT_TRUE(os.expired());
        EXPECT_FALSE(instance.expired());
        EXPECT_TRUE(saved.get_physical_device().owner_ref().managed());
        VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(saved).reset();
        EXPECT_TRUE(instance.expired());
    }
    TEST_F(StandardEngineVulkanIntegration, ClosingTheInitialWindowLeavesAnIndependentSurfaceUsable) {
        ASSERT_NO_THROW(start_engine());
        auto& first = *engine->boot_window;
        aho::Window second("independent startup Surface", 64, 48, *engine);
        const std::weak_ptr<vsl::impl::SurfaceImpl> previous = VSL_NAMESPACE::lifetime::detail::StartupAccess::Shared(*(first.data2->surface));
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(*(second.data2->surface)), VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(*(first.data2->surface)));
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
        EXPECT_TRUE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(EngineAccess::State(*(engine))->logical_device.get_physical_device().get_instance())->surface_capabilities2);
        EXPECT_TRUE(VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(EngineAccess::State(*(engine))->logical_device.get_physical_device().get_instance())->surface_maintenance1);
        EXPECT_TRUE(vsl::has_device_features(EngineAccess::State(*(engine))->logical_device.enabled_features(), vsl::DeviceFeature::SWAPCHAIN_MAINTENANCE1));
        EXPECT_TRUE(static_cast<bool>(engine->get_window()));
        auto& window = *engine->boot_window;
        EXPECT_EQ(EngineAccess::State(*(engine))->present_fences, vsl::PresentFencePolicy::REQUIRE_FENCE);
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
        const std::weak_ptr<aho::engine::EngineData> engine_data = EngineAccess::State(*(engine));
        const std::weak_ptr<vsl::PureWindow::WindowData> window_data = engine->boot_window->data_;
        VSL_NAMESPACE::lifetime::detail::StartupAccess::Owned(EngineAccess::State(*(engine))->logical_device)->device.waitIdle();
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
        EXPECT_EQ(EngineAccess::State(*(engine)), nullptr);
        ASSERT_NO_THROW(engine->boot("AHO deferred boot"));
        ASSERT_TRUE(engine->boot_window.has_value());
        EXPECT_EQ(engine->get_window().name(), "AHO deferred boot");
        EXPECT_EQ(EngineAccess::State(engine->get_window().engine), EngineAccess::State(*(engine)));
    }

    TEST_F(StandardEngineVulkanIntegration, MovedRegistrationAndHandleKeepTheDefaultAndRestoreThePreviousOne) {
        ASSERT_NO_THROW(start_engine());
        auto alternate = std::make_shared<aho::engine::EngineData>(*EngineAccess::State(*(engine)));
        {
            auto handle = EngineAccess::FromState(alternate);
            aho::engine::DefaultEngineRegistration registration(handle);
            auto moved_handle = std::move(handle);
            auto moved_registration = std::move(registration);
            EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), alternate);
            EXPECT_EQ(EngineAccess::State(moved_handle), alternate);
            EXPECT_FALSE(EngineAccess::State(handle));
        }
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), EngineAccess::State(*(engine)));
    }

    TEST_F(StandardEngineVulkanIntegration, RemovingAnOlderRegistrationKeepsTheNewestRegistration) {
        ASSERT_NO_THROW(start_engine());
        std::optional<aho::engine::DefaultEngineRegistration> older(std::in_place, *engine);
        const auto alternate = std::make_shared<aho::engine::EngineData>(*EngineAccess::State(*(engine)));
        std::optional<aho::engine::DefaultEngineRegistration> newer(std::in_place, EngineAccess::FromState(alternate));
        older.reset();
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), alternate);
        newer.reset();
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), EngineAccess::State(*(engine)));
    }

    TEST_F(StandardEngineVulkanIntegration, DefaultRegistrationRetainsItsDataUntilTheRegistrationEnds) {
        ASSERT_NO_THROW(start_engine());
        std::weak_ptr<aho::engine::EngineData> data;
        std::optional<aho::engine::DefaultEngineRegistration> registration;
        {
            auto alternate = std::make_shared<aho::engine::EngineData>(*EngineAccess::State(*(engine)));
            data = alternate;
            registration.emplace(EngineAccess::FromState(std::move(alternate)));
        }
        EXPECT_FALSE(data.expired());
        registration.reset();
        EXPECT_TRUE(data.expired());
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), EngineAccess::State(*(engine)));
    }

    TEST_F(StandardEngineVulkanIntegration, SharedHandleRetainsResourcesAfterTheOwnerUnregisters) {
        ASSERT_NO_THROW(start_engine());
        auto handle = aho::engine::EngineAccessor(*engine);
        const std::weak_ptr<aho::engine::EngineData> data = EngineAccess::State(handle);
        EngineAccess::State(*(engine))->command_manager.wait_idle();
        ASSERT_TRUE(engine->get_window().close());
        engine.reset();
        EXPECT_FALSE(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()));
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
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), EngineAccess::State(*(engine)));
        auto window = configured.get_window();
        EXPECT_EQ(window.name(), "configured window");
        EXPECT_EQ(window.window_size().value.___AN1, 72);
        EXPECT_EQ(window.window_size().value.___AN2, 48);
        const auto manager = std::dynamic_pointer_cast<vsl::graphic_resource::OverAllocateManager>(EngineAccess::State(configured)->graphic_resource_manager);
        ASSERT_NE(manager, nullptr);
        EXPECT_EQ(manager->size, config.engine.descriptor_budget);
        EXPECT_EQ(manager->other, 0u);
        EXPECT_TRUE(window.close());
    }

    TEST_F(StandardEngineVulkanIntegration, RepeatedBootPreservesTheExistingResourcesAndDefault) {
        ASSERT_NO_THROW(start_engine());
        const auto data = EngineAccess::State(*(engine));
        const auto windows = vsl::PureWindow::WINDOWS;
        EXPECT_THROW(engine->boot(), std::logic_error);
        EXPECT_THROW(engine->boot("replacement"), std::logic_error);
        EXPECT_EQ(EngineAccess::State(*(engine)), data);
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), data);
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
        EXPECT_FALSE(EngineAccess::State(failing));
        EXPECT_FALSE(failing.boot_window);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, windows);
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), EngineAccess::State(*(engine)));
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/engine/standard_engine.hpp>
namespace {
    TEST(StandardEngineTest, DeferredBootAllocatesNoGPUOrWindowAndGetWindowReportsAbsence) {
        const auto previous = EngineAccess::State(aho::engine::DefaultEngineRegistration::Current());
        {
            aho::StandardEngine engine("deferred test", false);
            EXPECT_EQ(EngineAccess::State(engine), nullptr);
            EXPECT_FALSE(engine.boot_window.has_value());
            EXPECT_THROW(engine.get_window(), std::bad_optional_access);
        }
        EXPECT_EQ(EngineAccess::State(aho::engine::DefaultEngineRegistration::Current()), previous);
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
        EXPECT_FALSE(EngineAccess::State(engine));
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
