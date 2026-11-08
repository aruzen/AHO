#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/storage/resource_layout.hpp>
#include <limits>

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
                l::construct_physical_device(storage, *source.accessor, candidate);
                ++constructed;
                l::set_physical_device_instance_owner(l::physical_device_at(storage), instance.alias(source.accessor.get()));
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
        vsl::PhysicalDevice empty(nullptr);
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
        auto selected = vsl::PhysicalDevice::Select(vsl::VulkanAccessor{instance});
        EXPECT_TRUE(selected.owner_ref().managed());
        EXPECT_EQ(selected.get_instance().accessor, instance);
        EXPECT_EQ(selected.data_->vulkan, instance.get());
        EXPECT_FALSE(selected.get_instance().accessor.owner_before(instance));
        EXPECT_FALSE(instance.owner_before(selected.get_instance().accessor));
        ASSERT_TRUE(selected.data_->props);
        ASSERT_TRUE(selected.data_->features);
        EXPECT_FALSE(selected.data_->mem_props);
        const auto expected = selected.data_->device.getProperties();
        EXPECT_EQ(selected.api_version(), expected.apiVersion);
        EXPECT_EQ(selected.name(), std::string(expected.deviceName));
        selected.data_->props.reset();
        EXPECT_EQ(selected.device_id(), expected.deviceID);
        EXPECT_TRUE(selected.data_->props);
        selected.data_->make_mem_props();
        ASSERT_TRUE(selected.data_->mem_props);
        EXPECT_EQ(selected.data_->mem_props->memoryTypeCount, selected.data_->device.getMemoryProperties().memoryTypeCount);
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, DefaultSnapshotSelectionRejectsEmptyListsAndUsesTheFirstNonnegativeCandidate) {
        vsl::PhysicalDevices<vsl::PhysicalDevicesSercher> listed(vsl::VulkanAccessor{instance});
        listed.data_->devices.assign(3, physical.data_->device);
        auto selected = listed.search();
        EXPECT_EQ(selected.data_->device, physical.data_->device);
        listed.data_->devices.clear();
        EXPECT_TRUE(listed.get().empty());
        EXPECT_THROW(listed.search(), vsl::exceptions::RuntimeException);
        listed.data_.reset();
        EXPECT_THROW(listed.get(), std::invalid_argument);
        EXPECT_THROW(listed.search(), std::invalid_argument);
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, CustomSearcherSavedCandidatesKeepTheirIdentityAfterEnumerationAndFacadeDestruction) {
        ResetSearcher cleanup;
        std::optional<vsl::PhysicalDevice> selected;
        std::weak_ptr<void> parent;
        {
            auto source = make_instance();
            parent = source.accessor;
            vsl::PhysicalDevices<RetainingSearcher> listed(source);
            ASSERT_FALSE(listed.data_->devices.empty());
            const auto handle = listed.data_->devices.front();
            listed.data_->devices.assign(3, handle);
            selected = listed.search();
            ASSERT_EQ(RetainingSearcher::SAVED.size(), 3u);
            EXPECT_EQ(selected->data_, RetainingSearcher::SAVED.back().data_);
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
        std::weak_ptr<void> child = selected.data_, parent = source.accessor;
        auto saved = selected.get_instance();
        EXPECT_EQ(saved.accessor, source.accessor);
        source = {};
        selected.data_.reset();
        EXPECT_TRUE(child.expired());
        EXPECT_FALSE(parent.expired());
        EXPECT_NO_THROW((void)saved.accessor->instance.enumeratePhysicalDevices());
        saved = {};
        EXPECT_TRUE(parent.expired());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, SameTypeSlotsAreDirectAndBorrowOneFixedParentOwner) {
        const auto candidate = o::inspect_physical_device(*instance, physical.data_->device);
        auto owner = std::make_shared<PairOwner>(vsl::VulkanAccessor{instance}, candidate);
        owner->node.publish(owner, owner.get(), PairOwner::VisitDependencies);
        auto& first = l::physical_device_at(owner->slot(0));
        auto& second = l::physical_device_at(owner->slot(1));
        first.owner_node = second.owner_node = &owner->node;
        vsl::PhysicalDevice a({owner, &first}), b({owner, &second});
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&second) - reinterpret_cast<std::uintptr_t>(&first), s::PHYSICAL_DEVICE_SLOT_SIZE);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&first) % s::PHYSICAL_DEVICE_SLOT_ALIGNMENT, 0u);
        EXPECT_EQ(a.get_instance().accessor, instance);
        EXPECT_EQ(b.get_instance().accessor, instance);
        EXPECT_FALSE(a.data_.owner_before(b.data_));
        EXPECT_FALSE(b.data_.owner_before(a.data_));
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
        a.data_.reset();
        EXPECT_FALSE(root.expired());
        EXPECT_EQ(b.device_id(), physical.device_id());
        b.data_.reset();
        EXPECT_TRUE(root.expired());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, OriginMismatchRollsBackTheSlotAndAValidCandidateCanReuseIt) {
        auto other = make_instance();
        const auto candidate = o::inspect_physical_device(*instance, physical.data_->device);
        SlotOwner slot;
        EXPECT_THROW(slot.construct(*other.accessor, candidate), std::invalid_argument);
        EXPECT_FALSE(slot.constructed);
        slot.construct(*instance, candidate);
        EXPECT_EQ(slot.value().device, physical.data_->device);
        EXPECT_EQ(slot.value().vulkan, instance.get());
        EXPECT_THROW(o::initialize_physical_device(slot.value(), *instance, candidate), std::logic_error);
        EXPECT_EQ(slot.value().device, physical.data_->device);
        EXPECT_EQ(slot.value().props->deviceID, physical.device_id());
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, ForeignOwnersAndNullNativeCandidatesAreRejectedBeforeQueryingTheDriver) {
        const auto candidate = o::inspect_physical_device(*instance, physical.data_->device);
        const vsl::VulkanAccessor forged{{std::make_shared<int>(0), instance.get()}};
        EXPECT_THROW(vsl::PhysicalDevice::Select(forged), std::invalid_argument);
        EXPECT_THROW(l::make_heap_physical_device(forged, candidate), std::invalid_argument);
        vsl::PhysicalDevice fake({std::make_shared<int>(0), physical.data_.get()});
        vsl::PhysicalDevice borrowed({std::shared_ptr<void>{}, physical.data_.get()});
        EXPECT_THROW(fake.owner_ref(), std::invalid_argument);
        EXPECT_THROW(fake.get_instance(), std::invalid_argument);
        EXPECT_THROW(borrowed.name(), std::invalid_argument);
        EXPECT_THROW(o::inspect_physical_device(*instance, {}), std::invalid_argument);
        const vk::PhysicalDevice invalid{reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{1})};
        EXPECT_THROW(o::inspect_physical_device(*instance, invalid), std::invalid_argument);
    }
    TEST_F(PhysicalDeviceStorageVulkanIntegrationTest, LogicalDeviceRetainsTheSelectedPhysicalDeviceAndInstanceAfterTheirFacadesAreGone) {
        std::weak_ptr<void> parent, selected_owner;
        std::shared_ptr<vsl::impl::LogicalDeviceImpl> logical;
        {
            auto source = make_instance();
            auto selected = vsl::PhysicalDevice::Select(source);
            parent = source.accessor;
            selected_owner = selected.data_;
            const auto queues = selected.data_->device.getQueueFamilyProperties();
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
