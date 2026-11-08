#include "support/window_fixture.hpp"
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/utils/typed_buffer_arena.hpp>
#include <limits>

namespace {
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    namespace t = vsl::typed;
    using StartupArenaVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    using StartupSurfaceVulkanIntegrationTest = vsl_test::WindowIntegrationTest;
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using Buffers = t::TypedBufferArena<t::BufferResource<t::ResourceId{1}, std::uint32_t, USAGE>>;
    using Native = vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE>;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    vsl::InstanceCreateInfo headless_info();

    vsl::InstanceCreateInfo headless_info() {
        vsl::InstanceCreateInfo info;
        info.application_name = "startup arena";
#ifdef __APPLE__
        info.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        return info;
    }
    TEST(StartupPlanTest, SameKindSlotsAreDistinctAndExternalEntriesHaveNoPlacement) {
        constexpr std::array requests{s::StartupRequest{{1}, s::StartupKind::DEVICE},
                                      s::StartupRequest{{2}, s::StartupKind::DEVICE},
                                      s::StartupRequest{{3}, s::StartupKind::INSTANCE, true}};
        constexpr s::StartupPlan<4> plan(requests);
        static_assert(plan.size() == 3 && plan.slots() == 2);
        static_assert(plan.placement({1}).offset == 0);
        static_assert(plan.placement({2}).offset >= s::DEVICE_SLOT_SIZE);
        static_assert(plan.placement({2}).offset % s::DEVICE_SLOT_ALIGNMENT == 0);
        static_assert(plan.bytes() % plan.alignment() == 0);
        static_assert(plan.placement({3}).offset == s::StartupPlacement::NO_SLOT);
        EXPECT_THROW(plan.require_region(plan.bytes() - 1, plan.alignment()), std::invalid_argument);
        EXPECT_THROW(plan.require_region(plan.bytes(), 3), std::invalid_argument);
        EXPECT_THROW(plan.placement({9}), std::out_of_range);
        EXPECT_THROW((s::StartupPlan<1>{requests}), std::length_error);
        const std::array duplicate{s::StartupRequest{{1}, s::StartupKind::DEVICE}, s::StartupRequest{{1}, s::StartupKind::INSTANCE}};
        EXPECT_THROW((s::StartupPlan<2>{duplicate}), std::invalid_argument);
        const std::array invalid{s::StartupRequest{{1}, static_cast<s::StartupKind>(99)}};
        EXPECT_THROW((s::StartupPlan<1>{invalid}), std::invalid_argument);
    }
    TEST(StartupArenaTest, EmptyPlanClaimsMemoryExclusivelyAndReleasesOnlyItsOwnClaim) {
        const std::array<s::StartupRequest, 0> requests{};
        const s::StartupPlan<0> plan(requests);
        const std::array<l::StartupInit, 0> inputs{};
        s::ArenaMemory memory(0, 1);
        {
            l::StartupArena first(plan, inputs, memory);
            EXPECT_EQ(first.size(), 0u);
            EXPECT_EQ(first.slots(), 0u);
            EXPECT_THROW((l::StartupArena{plan, inputs, memory}), std::invalid_argument);
            EXPECT_THROW((l::StartupArena{plan, inputs, memory}), std::invalid_argument);
            EXPECT_TRUE(first.owner_ref().managed());
        }
        EXPECT_NO_THROW((l::StartupArena{plan, inputs, memory}));
    }
    TEST(StartupArenaTest, InvalidInputKindAndReferencesAreRejectedBeforeNativeConstruction) {
        const std::array requests{s::StartupRequest{{1}, s::StartupKind::PHYSICAL_DEVICE}};
        const s::StartupPlan<1> plan(requests);
        const std::array<l::StartupInit, 1> wrong_kind{headless_info()};
        const std::array<l::StartupInit, 1> missing_parent{l::PhysicalDeviceInit{t::ResourceId{99}, {}}};
        const std::array<l::StartupInit, 1> wrong_parent{l::PhysicalDeviceInit{t::ResourceId{1}, {}}};
        const std::array<l::StartupInit, 0> missing_entry{};
        EXPECT_THROW((l::StartupArena{plan, wrong_kind}), std::invalid_argument);
        EXPECT_THROW((l::StartupArena{plan, missing_parent}), std::out_of_range);
        EXPECT_THROW((l::StartupArena{plan, wrong_parent}), std::invalid_argument);
        EXPECT_THROW((l::StartupArena{plan, missing_entry}), std::invalid_argument);
    }
    TEST_F(StartupArenaVulkanIntegrationTest, ReverseDeclarationBuildsParentsFirstAndSharesOneOwnerForTwoDevices) {
        const std::array requests{s::StartupRequest{{3}, s::StartupKind::DEVICE},
                                  s::StartupRequest{{4}, s::StartupKind::DEVICE},
                                  s::StartupRequest{{2}, s::StartupKind::PHYSICAL_DEVICE},
                                  s::StartupRequest{{1}, s::StartupKind::INSTANCE}};
        const s::StartupPlan<4> plan(requests);
        const std::array<l::StartupInit, 4> inputs{l::DeviceInit{t::ResourceId{2}, {}}, l::DeviceInit{t::ResourceId{2}, {}},
                                                   l::PhysicalDeviceInit{t::ResourceId{1}, {}}, headless_info()};
        l::WeakOwnerRef root;
        vsl::LogicalDeviceAccessor saved;
        {
            l::StartupArena arena(plan, inputs);
            root = arena.owner_ref().weak();
            EXPECT_EQ(arena.size(), 4u);
            EXPECT_EQ(arena.slots(), 4u);
            auto first = arena.device({3}), second = arena.device({4});
            EXPECT_NE(first.data_, second.data_);
            EXPECT_NE(first.data_->device, second.data_->device);
            EXPECT_FALSE(first.data_.owner_before(second.data_));
            EXPECT_FALSE(second.data_.owner_before(first.data_));
            EXPECT_EQ(first.get_physical_device().data_, arena.physical_device({2}).data_);
            EXPECT_EQ(first.get_physical_device().get_instance().accessor, arena.instance({1}).accessor);
            EXPECT_THROW(arena.instance({3}), std::invalid_argument);
            saved = first;
        }
        EXPECT_FALSE(root.expired());
        EXPECT_TRUE(saved.get_physical_device().get_instance().owner_ref().managed());
        saved.data_.reset();
        EXPECT_TRUE(root.expired());
    }
    TEST_F(StartupArenaVulkanIntegrationTest, ExternalGetterReturnsOriginalOwnerAndDoesNotRetainImporter) {
        const std::array requests{s::StartupRequest{{1}, s::StartupKind::INSTANCE, true},
                                  s::StartupRequest{{2}, s::StartupKind::PHYSICAL_DEVICE, true},
                                  s::StartupRequest{{3}, s::StartupKind::DEVICE, true}};
        const s::StartupPlan<3> plan(requests);
        const std::array<l::StartupInit, 3> inputs{vsl::VulkanAccessor{instance}, physical, device};
        vsl::LogicalDeviceAccessor saved;
        l::WeakOwnerRef importer;
        {
            l::StartupArena arena(plan, inputs);
            importer = arena.owner_ref().weak();
            EXPECT_EQ(arena.slots(), 0u);
            EXPECT_EQ(arena.size(), 3u);
            EXPECT_EQ(arena.instance({1}).accessor, instance);
            EXPECT_EQ(arena.physical_device({2}).data_, physical.data_);
            saved = arena.device({3});
            EXPECT_FALSE(saved.data_.owner_before(device.data_));
            EXPECT_FALSE(device.data_.owner_before(saved.data_));
        }
        EXPECT_TRUE(importer.expired());
        EXPECT_EQ(saved.data_, device.data_);
    }
    TEST_F(StartupArenaVulkanIntegrationTest, LaterDeviceFailureRollsBackAndAllowsTheSameMemoryToBeReused) {
        const std::array requests{s::StartupRequest{{1}, s::StartupKind::INSTANCE},
                                  s::StartupRequest{{2}, s::StartupKind::PHYSICAL_DEVICE},
                                  s::StartupRequest{{3}, s::StartupKind::DEVICE},
                                  s::StartupRequest{{4}, s::StartupKind::DEVICE}};
        const s::StartupPlan<4> plan(requests);
        s::ArenaMemory memory(plan.bytes() + 64, plan.alignment());
        vsl::DeviceCreateInfo bad;
        bad.features = static_cast<vsl::DeviceFeature>(1ull << 63);
        std::array<l::StartupInit, 4> inputs{headless_info(), l::PhysicalDeviceInit{t::ResourceId{1}, {}},
                                             l::DeviceInit{t::ResourceId{2}, {}}, l::DeviceInit{t::ResourceId{2}, bad}};
        EXPECT_THROW((l::StartupArena{plan, inputs, memory}), std::invalid_argument);
        inputs[3] = l::DeviceInit{t::ResourceId{2}, {}};
        vsl::LogicalDeviceAccessor saved;
        {
            l::StartupArena retry(plan, inputs, memory);
            EXPECT_EQ(retry.size(), plan.size());
            saved = retry.device({3});
        }
        EXPECT_THROW((l::StartupArena{plan, inputs, memory}), std::invalid_argument);
        saved.data_.reset();
        EXPECT_NO_THROW((l::StartupArena{plan, inputs, memory}));
    }
    TEST_F(StartupArenaVulkanIntegrationTest, ExternalInstanceIsACommonFixedParentRatherThanAnArenaSelfOwner) {
        const std::array requests{s::StartupRequest{{1}, s::StartupKind::PHYSICAL_DEVICE},
                                  s::StartupRequest{{2}, s::StartupKind::DEVICE}};
        const s::StartupPlan<2> plan(requests);
        const std::array<l::StartupInit, 2> inputs{l::PhysicalDeviceInit{vsl::VulkanAccessor{instance}, {}}, l::DeviceInit{t::ResourceId{1}, {}}};
        l::WeakOwnerRef root;
        {
            l::StartupArena arena(plan, inputs);
            root = arena.owner_ref().weak();
            const auto parent = arena.device({2}).get_physical_device().get_instance();
            EXPECT_EQ(parent.accessor, instance);
            EXPECT_FALSE(parent.accessor.owner_before(instance));
            EXPECT_FALSE(instance.owner_before(parent.accessor));
            EXPECT_TRUE(arena.owner_ref().owner_before(parent.accessor) || parent.accessor.owner_before(arena.owner_ref().share()));
        }
        EXPECT_TRUE(root.expired());
    }
    TEST_F(StartupArenaVulkanIntegrationTest, PublicStartupDeviceSupportsTypedCommandAfterArenaFacadeDestruction) {
        std::weak_ptr<void> startup_owner;
        {
            const std::array requests{s::StartupRequest{{1}, s::StartupKind::DEVICE}};
            const s::StartupPlan<1> plan(requests);
            const std::array<l::StartupInit, 1> inputs{l::DeviceInit{physical, {}}};
            vsl::LogicalDeviceAccessor placed;
            {
                l::StartupArena arena(plan, inputs);
                startup_owner = arena.owner_ref().share();
                placed = arena.device({1});
            }
            vsl::CommandManager manager(placed);
            Native destination(placed, 16);
            auto result = View::FromBuffer(destination);
            std::optional<vsl::command::CopyBuffer> command;
            std::weak_ptr<void> source_owner;
            {
                Buffers buffers(placed, Buffers::Plan({4}));
                source_owner = buffers.storage().owner();
                auto source = buffers.view<t::ResourceId{1}>();
                source.copy(std::array<std::uint32_t, 4>{3, 6, 9, 12});
                command.emplace(source.copy_to(result));
            }
            placed.data_.reset();
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << *command;
            command.reset();
            auto completion = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(completion.wait());
            EXPECT_TRUE(source_owner.expired());
            std::array<std::uint32_t, 4> actual{};
            result.read_into(actual);
            EXPECT_EQ(actual, (std::array<std::uint32_t, 4>{3, 6, 9, 12}));
            EXPECT_FALSE(startup_owner.expired());
        }
        EXPECT_TRUE(startup_owner.expired());
    }
    TEST_F(StartupSurfaceVulkanIntegrationTest, GraphicsDeviceDoesNotRetainItsCallOnlySurfaceOrOSWindow) {
        vsl::LogicalDeviceAccessor saved;
        l::WeakOwnerRef surface_root, os_root;
        {
            const std::array surface_requests{s::StartupRequest{{1}, s::StartupKind::SURFACE}};
            const s::StartupPlan<1> surface_plan(surface_requests);
            const std::array<l::StartupInit, 1> surface_inputs{l::SurfaceInit{vsl::VulkanAccessor{instance}, window->window_owner()}};
            l::StartupArena surfaces(surface_plan, surface_inputs);
            surface_root = surfaces.owner_ref().weak();
            os_root = window->window_owner().owner_ref().weak();
            const std::array device_requests{s::StartupRequest{{2}, s::StartupKind::PHYSICAL_DEVICE}, s::StartupRequest{{3}, s::StartupKind::DEVICE}};
            const s::StartupPlan<2> device_plan(device_requests);
            const std::array<l::StartupInit, 2> device_inputs{l::PhysicalDeviceInit{vsl::VulkanAccessor{instance}, {}},
                                                              l::GraphicsDeviceInit{t::ResourceId{2}, surfaces.surface({1}), false}};
            l::StartupArena devices(device_plan, device_inputs);
            saved = devices.device({3});
            EXPECT_EQ(saved.get_physical_device().get_instance().accessor, instance);
        }
        EXPECT_TRUE(surface_root.expired());
        EXPECT_TRUE(window->close());
        EXPECT_TRUE(os_root.expired());
        EXPECT_TRUE(saved.owner_ref().managed());
        Native buffer(saved, 16);
        EXPECT_TRUE(buffer.data_->buffer);
    }
    TEST_F(StartupSurfaceVulkanIntegrationTest, PlacedSurfacePluginReusesNativeAndRejectsAnotherOrUnmanagedWindow) {
        const std::array requests{s::StartupRequest{{1}, s::StartupKind::SURFACE}};
        const s::StartupPlan<1> plan(requests);
        const std::array<l::StartupInit, 1> inputs{l::SurfaceInit{vsl::VulkanAccessor{instance}, window->window_owner()}};
        l::StartupArena arena(plan, inputs);
        auto original = arena.surface({1});
        auto plugin = window->add_plugin<vsl::Surface>(original);
        EXPECT_EQ(plugin->data_, original.data_);
        EXPECT_EQ(plugin->data_->surface, original.data_->surface);
        auto fake_data = std::make_shared<vsl::PureWindow::WindowData>();
        fake_data->window_handle = window->data_->window_handle;
        vsl::PureWindow fake(fake_data);
        EXPECT_THROW(fake.add_plugin<vsl::Surface>(original), vsl::exceptions::RuntimeException);
        fake_data->window_handle = nullptr;
        EXPECT_TRUE(window->close());
        EXPECT_THROW(window->add_plugin<vsl::Surface>(original), std::invalid_argument);
        EXPECT_TRUE(original.get_window_owner());
    }
} // namespace
