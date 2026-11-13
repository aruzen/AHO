#include <VSL/lifetime/startup_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/storage/resource_layout.hpp>
#include <limits>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace {
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    namespace o = vsl::operation;
    struct SlotOwner {
        alignas(l::PhysicalDeviceSlotStorage) std::byte memory[sizeof(l::PhysicalDeviceSlotStorage)];
        bool constructed = false;
        ~SlotOwner();
        void construct(vsl::impl::VulkanImplAccessor& instance, const o::PhysicalDeviceCandidate& candidate);
        vsl::impl::PhysicalDeviceImpl& value() { return l::physical_device_at(memory); }
    };
    struct PairOwner {
        l::DependencyNode node;
        l::OwnerRef instance;
        alignas(l::PhysicalDeviceSlotStorage) std::byte memory[2 * sizeof(l::PhysicalDeviceSlotStorage)];
        size_t constructed = 0;
        PairOwner(vsl::VulkanAccessor source, const o::PhysicalDeviceCandidate& candidate);
        ~PairOwner();
        void clear() noexcept;
        void* slot(size_t index) { return memory + index * sizeof(l::PhysicalDeviceSlotStorage); }
        static void VisitDependencies(const void* owner, l::DependencyNode::EdgeVisitor visitor, void* context);
    };
    struct RetainingSearcher {
        static inline std::vector<vsl::PhysicalDevice> SAVED;
        int benchmark(vsl::PhysicalDevice device);
    };
    struct ResetSearcher {
        ~ResetSearcher() { RetainingSearcher::SAVED.clear(); }
    };
    class PhysicalDeviceStorageVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        vsl::VulkanAccessor make_instance() const;
    };
    SlotOwner::~SlotOwner() {
        if (constructed)
            l::destroy_physical_device(memory);
    }
    void SlotOwner::construct(vsl::impl::VulkanImplAccessor& instance, const o::PhysicalDeviceCandidate& candidate) {
        if (constructed)
            throw std::logic_error("Test slot is already constructed");
        l::construct_physical_device(memory, instance, candidate);
        constructed = true;
    }
    PairOwner::PairOwner(vsl::VulkanAccessor source, const o::PhysicalDeviceCandidate& candidate) : instance(source.owner_ref()) {
        try {
            while (constructed < 2) {
                auto* storage = slot(constructed);
                l::construct_physical_device(storage, *StartupAccess::Owned(source), candidate);
                ++constructed;
                l::set_physical_device_instance_owner(l::physical_device_at(storage), instance.alias(StartupAccess::Owned(source).get()));
            }
        } catch (...) {
            clear();
            throw;
        }
    }
    PairOwner::~PairOwner() {
        node.retire();
        clear();
    }
    void PairOwner::clear() noexcept {
        while (constructed)
            l::destroy_physical_device(slot(--constructed));
    }
    void PairOwner::VisitDependencies(const void* owner, l::DependencyNode::EdgeVisitor visitor, void* context) {
        visitor(context, static_cast<const PairOwner*>(owner)->instance);
    }
    int RetainingSearcher::benchmark(vsl::PhysicalDevice device) {
        SAVED.push_back(std::move(device));
        return static_cast<int>(SAVED.size());
    }
    vsl::VulkanAccessor PhysicalDeviceStorageVulkanIntegrationTest::make_instance() const {
        vsl::InstanceCreateInfo info;
        info.application_name = "physical device storage test";
        info.api_version = VK_API_VERSION_1_0;
        for (const auto& extension : vk::enumerateInstanceExtensionProperties())
            if (std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0 ||
                std::strcmp(extension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
                info.extensions.emplace_back(extension.extensionName);
        return vsl::VulkanAccessor::Create(std::move(info));
    }
    TEST(PhysicalDevicePlacementTest, ExactLayoutIncludesWeakOwnerContextAndDisallowsMovingImpls) {
        static_assert(sizeof(vsl::impl::PhysicalDeviceImpl) == s::PHYSICAL_DEVICE_IMPL_SIZE);
        static_assert(alignof(vsl::impl::PhysicalDeviceImpl) == s::PHYSICAL_DEVICE_IMPL_ALIGNMENT);
        static_assert(sizeof(l::PhysicalDeviceSlotStorage) == s::PHYSICAL_DEVICE_SLOT_SIZE);
        static_assert(alignof(l::PhysicalDeviceSlotStorage) == s::PHYSICAL_DEVICE_SLOT_ALIGNMENT);
        static_assert(!std::is_copy_constructible_v<vsl::impl::PhysicalDeviceImpl> && !std::is_move_constructible_v<vsl::impl::PhysicalDeviceImpl>);
        static_assert(s::ResourceSlotLayout<vsl::impl::PhysicalDeviceImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        EXPECT_GT(s::PHYSICAL_DEVICE_SLOT_SIZE, s::PHYSICAL_DEVICE_IMPL_SIZE);
        vsl::PhysicalDevice empty;
        EXPECT_THROW(empty.owner_ref(), std::invalid_argument);
        EXPECT_THROW(empty.get_instance(), std::invalid_argument);
        EXPECT_THROW(empty.name(), std::invalid_argument);
        EXPECT_THROW(vsl::PhysicalDevice::Select({}), std::invalid_argument);
    }
    TEST(PhysicalDevicePlacementTest, DefaultScorePreservesGeometryRuleDiscreteBonusAndSaturatesIntegerRange) {
        vk::PhysicalDeviceProperties properties{};
        vk::PhysicalDeviceFeatures features{};
        properties.deviceType = vk::PhysicalDeviceType::eDiscreteGpu;
        properties.limits.maxImageDimension2D = 4096;
        EXPECT_EQ(o::score_physical_device(properties, features), 0);
        features.geometryShader = true;
        EXPECT_EQ(o::score_physical_device(properties, features), 5096);
        properties.deviceType = vk::PhysicalDeviceType::eIntegratedGpu;
        EXPECT_EQ(o::score_physical_device(properties, features), 4096);
        properties.deviceType = vk::PhysicalDeviceType::eDiscreteGpu;
        properties.limits.maxImageDimension2D = std::numeric_limits<std::uint32_t>::max();
        EXPECT_EQ(o::score_physical_device(properties, features), std::numeric_limits<int>::max());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, SelectedHeapDeviceRetainsTheOriginalInstanceAndItsCapabilitySnapshot) {
        auto selected = vsl::PhysicalDevice::Select(vsl::lifetime::detail::StartupAccess::FromShared(instance));
        EXPECT_TRUE(selected.owner_ref().managed());
        EXPECT_EQ(StartupAccess::Owned(selected.get_instance()), instance);
        EXPECT_EQ(StartupAccess::Owned(selected)->vulkan, instance.get());
        EXPECT_FALSE(StartupAccess::Owned(selected.get_instance()).owner_before(instance));
        EXPECT_FALSE(instance.owner_before(StartupAccess::Shared(selected.get_instance())));
        ASSERT_TRUE(StartupAccess::Owned(selected)->props);
        ASSERT_TRUE(StartupAccess::Owned(selected)->features);
        EXPECT_FALSE(StartupAccess::Owned(selected)->mem_props);
        const auto expected = StartupAccess::Owned(selected)->device.getProperties();
        EXPECT_EQ(selected.api_version(), expected.apiVersion);
        EXPECT_EQ(selected.name(), std::string(expected.deviceName));
        StartupAccess::Owned(selected)->props.reset();
        EXPECT_EQ(selected.device_id(), expected.deviceID);
        EXPECT_TRUE(StartupAccess::Owned(selected)->props);
        StartupAccess::Owned(selected)->make_mem_props();
        ASSERT_TRUE(StartupAccess::Owned(selected)->mem_props);
        EXPECT_EQ(StartupAccess::Owned(selected)->mem_props->memoryTypeCount, StartupAccess::Owned(selected)->device.getMemoryProperties().memoryTypeCount);
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, DefaultSnapshotSelectionRejectsEmptyListsAndUsesTheFirstNonnegativeCandidate) {
        vsl::PhysicalDevices<vsl::PhysicalDevicesSercher> listed(vsl::lifetime::detail::StartupAccess::FromShared(instance));
        StartupAccess::Collection(listed)->devices.assign(3, StartupAccess::Owned(physical)->device);
        auto selected = listed.search();
        EXPECT_EQ(StartupAccess::Owned(selected)->device, StartupAccess::Owned(physical)->device);
        StartupAccess::Collection(listed)->devices.clear();
        EXPECT_TRUE(listed.get().empty());
        EXPECT_THROW(listed.search(), vsl::exceptions::RuntimeException);
        StartupAccess::Collection(listed).reset();
        EXPECT_THROW(listed.get(), std::invalid_argument);
        EXPECT_THROW(listed.search(), std::invalid_argument);
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, CustomSearcherSavedCandidatesKeepTheirIdentityAfterEnumerationAndFacadeDestruction) {
        ResetSearcher cleanup;
        std::optional<vsl::PhysicalDevice> selected;
        std::weak_ptr<void> parent;
        {
            auto source = make_instance();
            parent = StartupAccess::Shared(source);
            vsl::PhysicalDevices<RetainingSearcher> listed(source);
            ASSERT_FALSE(StartupAccess::Collection(listed)->devices.empty());
            const auto handle = StartupAccess::Collection(listed)->devices.front();
            StartupAccess::Collection(listed)->devices.assign(3, handle);
            selected = listed.search();
            ASSERT_EQ(RetainingSearcher::SAVED.size(), 3u);
            EXPECT_EQ(StartupAccess::Owned(*(selected)), StartupAccess::Owned(RetainingSearcher::SAVED.back()));
        }
        EXPECT_FALSE(parent.expired());
        EXPECT_TRUE(selected->get_instance().owner_ref().managed());
        EXPECT_FALSE(selected->name().empty());
        RetainingSearcher::SAVED.clear();
        EXPECT_FALSE(parent.expired());
        selected.reset();
        EXPECT_TRUE(parent.expired());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, InstanceGetterKeepsTheParentWithoutKeepingTheChildAlive) {
        auto source = make_instance();
        auto selected = vsl::PhysicalDevice::Select(source);
        std::weak_ptr<void> child = StartupAccess::Shared(selected), parent = StartupAccess::Shared(source);
        auto saved = selected.get_instance();
        EXPECT_EQ(StartupAccess::Owned(saved), StartupAccess::Owned(source));
        source = {};
        StartupAccess::Owned(selected).reset();
        EXPECT_TRUE(child.expired());
        EXPECT_FALSE(parent.expired());
        EXPECT_NO_THROW((void)StartupAccess::Owned(saved)->instance.enumeratePhysicalDevices());
        saved = {};
        EXPECT_TRUE(parent.expired());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, SameTypeSlotsAreDirectAndBorrowOneFixedParentOwner) {
        const auto candidate = o::inspect_physical_device(*instance, StartupAccess::Owned(physical)->device);
        auto owner = std::make_shared<PairOwner>(vsl::lifetime::detail::StartupAccess::FromShared(instance), candidate);
        owner->node.publish(owner, owner.get(), PairOwner::VisitDependencies);
        auto& first = l::physical_device_at(owner->slot(0));
        auto& second = l::physical_device_at(owner->slot(1));
        first.owner_node = second.owner_node = &owner->node;
        auto a = StartupAccess::FromShared(std::shared_ptr<vsl::impl::PhysicalDeviceImpl>{owner, &first});
        auto b = StartupAccess::FromShared(std::shared_ptr<vsl::impl::PhysicalDeviceImpl>{owner, &second});
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&second) - reinterpret_cast<std::uintptr_t>(&first), s::PHYSICAL_DEVICE_SLOT_SIZE);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&first) % s::PHYSICAL_DEVICE_SLOT_ALIGNMENT, 0u);
        EXPECT_EQ(StartupAccess::Owned(a.get_instance()), instance);
        EXPECT_EQ(StartupAccess::Owned(b.get_instance()), instance);
        EXPECT_FALSE(StartupAccess::Owned(a).owner_before(StartupAccess::Owned(b)));
        EXPECT_FALSE(StartupAccess::Owned(b).owner_before(StartupAccess::Owned(a)));
        EXPECT_TRUE(a.owner_ref().managed());
        EXPECT_TRUE(b.owner_ref().managed());
        std::vector<l::OwnerRef> parents;
        owner->node.visit_edges(+[](void* context, const l::OwnerRef& edge) { static_cast<std::vector<l::OwnerRef>*>(context)->push_back(edge); }, &parents);
        ASSERT_EQ(parents.size(), 1u);
        EXPECT_TRUE(parents[0].managed());
        EXPECT_FALSE(parents[0].share().owner_before(instance));
        EXPECT_FALSE(instance.owner_before(parents[0].share()));
        std::weak_ptr<void> root = owner;
        owner.reset();
        StartupAccess::Owned(a).reset();
        EXPECT_FALSE(root.expired());
        EXPECT_EQ(b.device_id(), physical.device_id());
        StartupAccess::Owned(b).reset();
        EXPECT_TRUE(root.expired());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, OriginMismatchRollsBackTheSlotAndAValidCandidateCanReuseIt) {
        auto other = make_instance();
        const auto candidate = o::inspect_physical_device(*instance, StartupAccess::Owned(physical)->device);
        SlotOwner slot;
        EXPECT_THROW(slot.construct(*StartupAccess::Owned(other), candidate), std::invalid_argument);
        EXPECT_FALSE(slot.constructed);
        slot.construct(*instance, candidate);
        EXPECT_EQ(slot.value().device, StartupAccess::Owned(physical)->device);
        EXPECT_EQ(slot.value().vulkan, instance.get());
        EXPECT_THROW(o::initialize_physical_device(slot.value(), *instance, candidate), std::logic_error);
        EXPECT_EQ(slot.value().device, StartupAccess::Owned(physical)->device);
        EXPECT_EQ(slot.value().props->deviceID, physical.device_id());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, ForeignOwnersAndNullNativeCandidatesAreRejectedBeforeQueryingTheDriver) {
        const auto candidate = o::inspect_physical_device(*instance, StartupAccess::Owned(physical)->device);
        const auto forged = vsl_test::unchecked_startup(std::shared_ptr<vsl::impl::VulkanImplAccessor>{std::make_shared<int>(0), instance.get()});
        EXPECT_THROW(vsl::PhysicalDevice::Select(forged), std::invalid_argument);
        EXPECT_THROW(l::make_heap_physical_device(forged, candidate), std::invalid_argument);
        const auto fake = vsl_test::unchecked_startup(std::shared_ptr<vsl::impl::PhysicalDeviceImpl>{std::make_shared<int>(0), StartupAccess::Owned(physical).get()});
        auto borrowed = vsl_test::unchecked_startup(std::shared_ptr<vsl::impl::PhysicalDeviceImpl>{std::shared_ptr<void>{}, StartupAccess::Owned(physical).get()});
        EXPECT_THROW(fake.owner_ref(), std::invalid_argument);
        EXPECT_THROW(fake.get_instance(), std::invalid_argument);
        EXPECT_THROW(borrowed.name(), std::invalid_argument);
        EXPECT_THROW(StartupAccess::FromShared(StartupAccess::Shared(fake)), std::invalid_argument);
        EXPECT_THROW(StartupAccess::FromShared(StartupAccess::Shared(borrowed)), std::invalid_argument);
        l::DependencyNode different_node;
        different_node.publish(StartupAccess::Shared(physical));
        EXPECT_THROW(StartupAccess::FromOwned(different_node.ref(StartupAccess::Shared(physical))), std::invalid_argument);
        EXPECT_THROW(o::inspect_physical_device(*instance, {}), std::invalid_argument);
        const vk::PhysicalDevice invalid{reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{1})};
        EXPECT_THROW(o::inspect_physical_device(*instance, invalid), std::invalid_argument);
        const auto handle = StartupAccess::Owned(physical)->device;
        StartupAccess::Owned(physical)->owner_node->retire();
        EXPECT_THROW(physical.owner_ref(), std::invalid_argument);
        EXPECT_THROW(StartupAccess::FromOwned(StartupAccess::Owned(physical)), std::invalid_argument);
        EXPECT_THROW(StartupAccess::FromShared(StartupAccess::Shared(physical)), std::invalid_argument);
        EXPECT_EQ(StartupAccess::Owned(physical)->device, handle);
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, LogicalDeviceRetainsTheSelectedPhysicalDeviceAndInstanceAfterTheirFacadesAreGone) {
        std::weak_ptr<void> parent, selected_owner;
        std::shared_ptr<vsl::impl::LogicalDeviceImpl> logical;
        {
            auto source = make_instance();
            auto selected = vsl::PhysicalDevice::Select(source);
            parent = StartupAccess::Shared(source);
            selected_owner = StartupAccess::Shared(selected);
            const auto queues = StartupAccess::Owned(selected)->device.getQueueFamilyProperties();
            const auto found = std::ranges::find_if(queues, [](const auto& queue) { return bool(queue.queueFlags & vk::QueueFlagBits::eGraphics); });
            if (found == queues.end())
                GTEST_SKIP() << "Selected physical device has no graphics queue";
            const auto family = static_cast<std::uint32_t>(found - queues.begin());
            vsl::DeviceCreateInfo info;
            info.graphics_family = family;
            info.require_compute = false;
            logical = vsl::LogicalDeviceAccessor::Create(selected, std::move(info)).data_;
        }
        EXPECT_FALSE(parent.expired());
        EXPECT_FALSE(selected_owner.expired());
        EXPECT_TRUE(logical->device.getQueue(logical->graphics_family, 0));
        EXPECT_TRUE(vsl::LogicalDeviceAccessor{logical}.get_physical_device().get_instance().owner_ref().managed());
        logical.reset();
        EXPECT_TRUE(parent.expired());
        EXPECT_TRUE(selected_owner.expired());
    }
} // namespace
