#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/sampler_access.hpp>
#include "support/vulkan_fixture.hpp"
#include "support/fixed_parent.hpp"
#include <VSL/lifetime/device_storage.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/storage/resource_layout.hpp>
#include <VSL/vulkan/sampler.hpp>
#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <VSL/utils/spir_reflector.hpp>
#include <limits>

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
    using VSL_NAMESPACE::lifetime::detail::SamplerAccess;
} // namespace

namespace {
    namespace l = vsl::lifetime;
    namespace o = vsl::operation;
    namespace s = vsl::storage;
    namespace t = vsl::typed;
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using Buffers = t::TypedBufferArena<t::BufferResource<t::ResourceId{1}, std::uint32_t, USAGE>,
                                        t::BufferResource<t::ResourceId{2}, std::uint32_t, USAGE>>;
    using Native = vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE>;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    class DeviceStorageVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    struct DeviceProbe {
        unsigned creates = 0, destroys = 0, fail_on = 1;
        bool fail = true;
        bool empty = false;
        static VkResult Create(void* context, VkPhysicalDevice physical, const VkDeviceCreateInfo* info, VkDevice* device);
        static void Destroy(void* context, VkDevice device) noexcept;
        o::DeviceNativeCalls calls() { return {this, Create, Destroy}; }
    };
    struct DeviceSlot {
        alignas(l::DeviceSlotStorage) std::byte memory[sizeof(l::DeviceSlotStorage)];
        bool constructed = false;
        o::DeviceNativeCalls calls;
        ~DeviceSlot();
        void construct(vsl::impl::PhysicalDeviceImpl& physical, vsl::DeviceCreateInfo info);
        vsl::impl::LogicalDeviceImpl& value() { return l::device_at(memory); }
    };
    struct DevicePair {
        l::DependencyNode node;
        l::OwnerRef physical;
        o::DeviceNativeCalls calls;
        alignas(l::DeviceSlotStorage) std::byte memory[2 * sizeof(l::DeviceSlotStorage)];
        size_t constructed = 0;
        DevicePair(vsl::PhysicalDevice physical, const o::DeviceNativeCalls& calls = {});
        ~DevicePair();
        void clear() noexcept;
        void* slot(size_t index) { return memory + index * sizeof(l::DeviceSlotStorage); }
        void publish(const std::shared_ptr<DevicePair>& owner);
        static void VisitDependencies(const void* root, l::DependencyNode::EdgeVisitor visitor, void* context);
    };
    struct DeviceAndBuffer {
        l::DependencyNode node;
        l::OwnerRef physical;
        alignas(l::DeviceSlotStorage) std::byte device_memory[sizeof(l::DeviceSlotStorage)];
        alignas(l::BufferSlotStorage) std::byte buffer_memory[sizeof(l::BufferSlotStorage)];
        explicit DeviceAndBuffer(vsl::PhysicalDevice physical);
        ~DeviceAndBuffer();
        void publish(const std::shared_ptr<DeviceAndBuffer>& owner);
        static void VisitDependencies(const void* root, l::DependencyNode::EdgeVisitor visitor, void* context);
    };
    VkResult DeviceProbe::Create(void* context, VkPhysicalDevice physical, const VkDeviceCreateInfo* info, VkDevice* device) {
        auto& probe = *static_cast<DeviceProbe*>(context);
        ++probe.creates;
        if (probe.empty) {
            *device = VK_NULL_HANDLE;
            return VK_SUCCESS;
        }
        if (probe.fail && probe.creates == probe.fail_on) {
            *device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        return vkCreateDevice(physical, info, nullptr, device);
    }
    void DeviceProbe::Destroy(void* context, VkDevice device) noexcept {
        ++static_cast<DeviceProbe*>(context)->destroys;
        vkDestroyDevice(device, nullptr);
    }
    DeviceSlot::~DeviceSlot() {
        if (constructed) {
            o::destroy_device(value(), calls);
            l::destroy_device(memory);
        }
    }
    void DeviceSlot::construct(vsl::impl::PhysicalDeviceImpl& physical, vsl::DeviceCreateInfo info) {
        if (constructed)
            throw std::logic_error("Test Device slot is already constructed");
        l::construct_device(memory, physical, std::move(info), calls);
        constructed = true;
    }
    DevicePair::DevicePair(vsl::PhysicalDevice source, const o::DeviceNativeCalls& calls) : physical(source.owner_ref()), calls(calls) {
        try {
            while (constructed < 2) {
                const auto index = constructed;
                l::construct_device(slot(index), *source.data_, {}, calls);
                ++constructed;
                l::set_device_physical_owner(l::device_at(slot(index)), physical.alias(source.data_.get()));
            }
        } catch (...) {
            clear();
            throw;
        }
    }
    DevicePair::~DevicePair() {
        node.retire();
        clear();
    }
    void DevicePair::clear() noexcept {
        while (constructed) {
            auto* memory = slot(--constructed);
            o::destroy_device(l::device_at(memory), calls);
            l::destroy_device(memory);
        }
    }
    void DevicePair::publish(const std::shared_ptr<DevicePair>& owner) {
        node.publish(owner, this, VisitDependencies);
        for (size_t i = 0; i < constructed; ++i)
            l::device_at(slot(i)).owner_node = &node;
    }
    void DevicePair::VisitDependencies(const void* root, l::DependencyNode::EdgeVisitor visitor, void* context) {
        visitor(context, static_cast<const DevicePair*>(root)->physical);
    }
    DeviceAndBuffer::DeviceAndBuffer(vsl::PhysicalDevice source) : physical(source.owner_ref()) {
        l::construct_device(device_memory, *source.data_, {});
        try {
            auto& device = l::device_at(device_memory);
            l::set_device_physical_owner(device, physical.alias(source.data_.get()));
            l::construct_buffer(buffer_memory, device, vsl::BufferCreateInfo{4, vsl::MemoryType::UNIFORM_BUFFER});
        } catch (...) {
            l::destroy_device(device_memory);
            throw;
        }
    }
    DeviceAndBuffer::~DeviceAndBuffer() {
        node.retire();
        l::destroy_buffer(buffer_memory);
        l::destroy_device(device_memory);
    }
    void DeviceAndBuffer::publish(const std::shared_ptr<DeviceAndBuffer>& owner) {
        node.publish(owner, this, VisitDependencies);
        l::device_at(device_memory).owner_node = &node;
        l::detail::BufferAccess::Impl(l::buffer_at(buffer_memory))->owner_node = &node;
    }
    void DeviceAndBuffer::VisitDependencies(const void* root, l::DependencyNode::EdgeVisitor visitor, void* context) {
        visitor(context, static_cast<const DeviceAndBuffer*>(root)->physical);
    }
    TEST(DeviceStorageTest, ActualLayoutSeparatesWeakParentContextAndHasNoGpuUseState) {
        static_assert(sizeof(vsl::impl::LogicalDeviceImpl) == s::DEVICE_IMPL_SIZE);
        static_assert(sizeof(l::DeviceSlotStorage) == s::DEVICE_SLOT_SIZE);
        static_assert(alignof(l::DeviceSlotStorage) == s::DEVICE_SLOT_ALIGNMENT);
        static_assert(s::ResourceSlotLayout<vsl::impl::LogicalDeviceImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(!std::is_copy_constructible_v<vsl::impl::LogicalDeviceImpl> && !std::is_move_constructible_v<vsl::impl::LogicalDeviceImpl>);
        EXPECT_GT(s::DEVICE_SLOT_SIZE, s::DEVICE_IMPL_SIZE);
        const vsl::LogicalDeviceAccessor empty;
        EXPECT_THROW(empty.owner_ref(), std::invalid_argument);
        EXPECT_THROW(empty.get_physical_device(), std::invalid_argument);
        EXPECT_THROW(empty.enabled_features(), std::invalid_argument);
        EXPECT_THROW(vsl::LogicalDeviceAccessor::Create(vsl::PhysicalDevice(nullptr)), std::invalid_argument);
        EXPECT_THROW(vsl::LogicalDevice<>(vsl::PhysicalDevice(nullptr), std::shared_ptr<vsl::Surface>{}), std::invalid_argument);
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, DistinctDeviceSlotsShareOneOwnerAndReturnTheOriginalPhysicalParent) {
        auto root = std::make_shared<DevicePair>(physical);
        root->publish(root);
        vsl::LogicalDeviceAccessor first{{root, &l::device_at(root->slot(0))}}, second{{root, &l::device_at(root->slot(1))}};
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second.data_.get()) - reinterpret_cast<std::uintptr_t>(first.data_.get()), s::DEVICE_SLOT_SIZE);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first.data_.get()) % s::DEVICE_SLOT_ALIGNMENT, 0u);
        EXPECT_NE(first.data_->device, second.data_->device);
        EXPECT_FALSE(first.data_.owner_before(second.data_));
        EXPECT_FALSE(second.data_.owner_before(first.data_));
        const auto original = first.get_physical_device();
        vsl_test::expect_fixed_parent(first.owner_ref(), physical.owner_ref());
        EXPECT_EQ(original.data_, physical.data_);
        EXPECT_FALSE(original.data_.owner_before(physical.data_));
        EXPECT_FALSE(physical.data_.owner_before(original.data_));
        std::weak_ptr<void> owner = root;
        root.reset();
        first.data_.reset();
        EXPECT_FALSE(owner.expired());
        second.data_.reset();
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(original.owner_ref().managed());
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, FailedNativeOutputIsNotOwnedAndTheSameSlotCanBeRetried) {
        DeviceProbe probe;
        DeviceSlot slot;
        slot.calls = probe.calls();
        EXPECT_THROW(slot.construct(*physical.data_, {}), vk::OutOfHostMemoryError);
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(probe.destroys, 0u);
        probe.fail = false;
        slot.construct(*physical.data_, {});
        EXPECT_TRUE(slot.value().device);
        EXPECT_THROW(slot.construct(*physical.data_, {}), std::logic_error);
        EXPECT_THROW(o::initialize_device(slot.value(), *physical.data_, {}), std::logic_error);
        o::destroy_device(slot.value(), slot.calls);
        o::destroy_device(slot.value(), slot.calls);
        EXPECT_EQ(probe.creates, 2u);
        EXPECT_EQ(probe.destroys, 1u);
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, LaterFailureRollsBackOnlyTheSuccessfullyCreatedDevice) {
        DeviceProbe probe;
        probe.fail_on = 2;
        EXPECT_THROW(DevicePair(physical, probe.calls()), vk::OutOfHostMemoryError);
        EXPECT_EQ(probe.creates, 2u);
        EXPECT_EQ(probe.destroys, 1u);
        EXPECT_NO_THROW(vsl::LogicalDeviceAccessor::Create(physical));
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, EmptySuccessfulOutputDoesNotConstructTheSlotAndAllowsRetry) {
        DeviceProbe probe;
        probe.empty = true;
        DeviceSlot slot;
        slot.calls = probe.calls();
        EXPECT_THROW(slot.construct(*physical.data_, {}), std::runtime_error);
        EXPECT_FALSE(slot.constructed);
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(probe.destroys, 0u);
        probe.empty = probe.fail = false;
        EXPECT_NO_THROW(slot.construct(*physical.data_, {}));
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, InvalidQueuesExtensionsAndFeatureBitsFailBeforeNativeCreation) {
        DeviceProbe probe;
        DeviceSlot slot;
        slot.calls = probe.calls();
        vsl::DeviceCreateInfo info;
        info.graphics_family = std::numeric_limits<std::uint32_t>::max();
        EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        info.graphics_family = queue_family;
        info.queues = {{queue_family, {std::numeric_limits<float>::quiet_NaN()}}};
        EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        info.queues = {{queue_family}, {queue_family}};
        EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        info.queues = {{queue_family, {}}};
        EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        info.queues.clear();
        info.extensions = {"VK_VSL_missing_extension"};
        EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        info.extensions.clear();
        info.features = static_cast<vsl::DeviceFeature>(std::uint64_t{1} << 63);
        EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        const auto unsupported = ~static_cast<std::uint64_t>(physical.supported_features()) & ((std::uint64_t{1} << 55) - 1);
        if (unsupported) {
            info.features = static_cast<vsl::DeviceFeature>(unsupported & (~unsupported + 1));
            EXPECT_THROW(slot.construct(*physical.data_, info), std::invalid_argument);
        }
        EXPECT_EQ(probe.creates, 0u);
        EXPECT_EQ(probe.destroys, 0u);
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, SupportedFeaturesDoNotImplicitlyBecomeEnabled) {
        auto minimal = vsl::LogicalDeviceAccessor::Create(physical);
        EXPECT_EQ(minimal.enabled_features(), vsl::DeviceFeature::NONE);
        EXPECT_EQ(minimal.data_->graphics_family, minimal.data_->present_family);
        EXPECT_TRUE(minimal.data_->device.getQueue(minimal.data_->present_family, 0));
        EXPECT_THROW((vsl::Sampler(minimal, vsl::SamplingMode::LINEAR)), std::invalid_argument);
        EXPECT_NO_THROW((vsl::Sampler(minimal, vsl::SamplingMode::NEAREST)));
        EXPECT_TRUE(vsl::has_device_features(physical.supported_features(), minimal.enabled_features()));
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, ChildPlacementCanUseAnUnpublishedDeviceWithoutAnOwnerAdapter) {
        DeviceSlot slot;
        slot.construct(*physical.data_, {});
        ASSERT_EQ(slot.value().owner_node, nullptr);
        alignas(l::BufferSlotStorage) std::byte memory[sizeof(l::BufferSlotStorage)];
        l::construct_buffer(memory, slot.value(), vsl::BufferCreateInfo{16, USAGE});
        const auto cleanup = std::unique_ptr<void, void (*)(void*)>(memory, l::destroy_buffer);
        const auto buffer = l::buffer_at(memory);
        const std::array<std::uint32_t, 4> values{11, 22, 33, 44};
        buffer.write(std::as_bytes(std::span(values)));
        std::array<std::uint32_t, 4> actual{};
        buffer.read_into(std::as_writable_bytes(std::span(actual)));
        EXPECT_EQ(actual, values);
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, ForgedOrBorrowedDeviceOwnersAreRejectedBeforeCreatingChildren) {
        const vsl::LogicalDeviceAccessor borrowed{{std::shared_ptr<void>{}, device.data_.get()}};
        const vsl::LogicalDeviceAccessor forged{{std::make_shared<int>(0), device.data_.get()}};
        for (const auto& source : {borrowed, forged}) {
            EXPECT_THROW(source.owner_ref(), std::invalid_argument);
            EXPECT_THROW(source.get_physical_device(), std::invalid_argument);
            EXPECT_THROW(source.enabled_features(), std::invalid_argument);
            EXPECT_THROW(Native(source, 4), std::invalid_argument);
            EXPECT_THROW((vsl::PipelineLayout(source)), std::invalid_argument);
            EXPECT_THROW((vsl::CommandManager(source)), std::invalid_argument);
            EXPECT_THROW((vsl::SynchroManager(source)), std::invalid_argument);
            EXPECT_THROW((vsl::graphic_resource::ExactAllocateManager(source)), std::invalid_argument);
            EXPECT_THROW((vsl::graphic_resource::OverAllocateManager(source)), std::invalid_argument);
            const std::array<std::uint32_t, 5> header{0x07230203, 0x10000, 0, 1, 0};
            EXPECT_THROW(vsl::utils::SPIRVReflector(source, std::span(header)), std::invalid_argument);
        }
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, NativeLeavesAndBothArenasExposeTheirActualFixedDeviceParent) {
        Native buffer(device, 16);
        vsl::Image<vsl::ImageType::SAMPLED> image(device, 2, 2);
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST);
        vsl::Shader<vsl::ShaderType::VERTEX> shader(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX);
        vsl::graphic_resource::Pool pool{l::make_heap_descriptor_pool(device, {{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1})};
        auto [allocated, set] = pool.bind(layout);
        ASSERT_TRUE(allocated);
        const auto pipeline_layout = l::make_heap_pipeline_layout(device, {}, {});
        vsl::RenderPass pass(device, vsl::RenderPassDescription{});
        vsl::CommandManager manager(device);
        vsl::SynchroManager synchronization(device);
        auto fence = synchronization.create_fence("fixed Device fence");
        auto semaphore = synchronization.create_semaphore("fixed Device semaphore");
        Buffers buffers(device, Buffers::Plan({4, 4}));
        constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<1> plan(requests);
        const std::array<l::ResourceInit, 1> initializers{vsl::BufferCreateInfo{16, USAGE}};
        l::ResourceArena resources(device, plan, initializers);
        const std::array<std::pair<const char*, l::OwnerRef>, 13> owners{{{"Buffer", vsl_test::canonical_owner(BufferAccess::Owned(buffer))}, {"Image", vsl_test::canonical_owner(ImageAccess::Owned(image))}, {"Sampler", vsl_test::canonical_owner(SamplerAccess::Owned(sampler))}, {"Shader", shader.owner_ref()}, {"DescriptorLayout", vsl_test::canonical_owner(layout.data_)}, {"DescriptorPool", vsl_test::canonical_owner(pool.data_)}, {"PipelineLayout", vsl_test::canonical_owner(pipeline_layout)}, {"RenderPass", pass.owner_ref()}, {"CommandPool", manager.get_pool().owner_ref()}, {"Fence", fence.owner_ref()}, {"Semaphore", semaphore.owner_ref()}, {"BufferArena", buffers.view<t::ResourceId{1}>().storage_view().owner_ref()}, {"ResourceArena", resources.owner_ref()}}};
        for (const auto& [name, owner] : owners) {
            SCOPED_TRACE(name);
            vsl_test::expect_fixed_parent(owner, device.owner_ref());
        }
        vsl_test::expect_fixed_parent(set.owner_ref(), device.owner_ref(), false);
        vsl_test::expect_fixed_parent(manager.get_buffer().owner_ref(), device.owner_ref(), false);
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, FixedDescriptorBorrowsAResourceAlreadyKeptByItsDeviceRoot) {
        std::weak_ptr<void> owner;
        {
            auto root = std::make_shared<DeviceAndBuffer>(physical);
            root->publish(root);
            owner = root;
            vsl::LogicalDeviceAccessor placed{{root, &l::device_at(root->device_memory)}};
            std::optional<vsl::BufferView> buffer;
            buffer.emplace(l::detail::BufferAccess::View(l::buffer_at(root->buffer_memory), root->node.ref(root), 0, 4));
            constexpr std::array<t::ResourceRequest, 3> requests{{{{1}, s::ResourceKind::DESCRIPTOR_LAYOUT}, {{2}, s::ResourceKind::DESCRIPTOR_POOL}, {{3}, s::ResourceKind::DESCRIPTOR_SET}}};
            constexpr t::ResourcePlan<3> plan(requests);
            const std::array<l::ResourceInit, 3> initializers{
                vsl::DescriptorLayoutCreateInfo{{{0, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE}}},
                vsl::DescriptorPoolCreateInfo{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1}, l::DescriptorSetInit{{2}, {1}}};
            l::ResourceArena descriptors(placed, plan, initializers);
            auto set = descriptors.descriptor_set({3});
            {
                using Spec = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
                using Uniform = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
                auto typed = t::TypedDescriptorSet<Spec>::FromResource(placed, set);
                EXPECT_NO_THROW(typed.write<0>(Uniform::FromView(*buffer, 1)));
            }
            ASSERT_TRUE(set.data_->bindings.at(0).front().initialized);
            EXPECT_TRUE(set.data_->bindings.at(0).front().owners.empty());
            EXPECT_TRUE(set.data_->bindings.at(0).front().constraint.owners.empty());
            buffer.reset();
            placed.data_.reset();
            root.reset();
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, ChildGetterKeepsTheOriginalDeviceWithoutKeepingTheChildRootAlive) {
        auto root = std::make_shared<DevicePair>(physical);
        root->publish(root);
        const vsl::LogicalDeviceAccessor placed{{root, &l::device_at(root->slot(0))}};
        std::weak_ptr<void> child;
        vsl::LogicalDeviceAccessor parent;
        {
            Native buffer(placed, 4);
            child = BufferAccess::Owned(buffer).share();
            parent = buffer.get_device();
            EXPECT_EQ(parent.data_, placed.data_);
            EXPECT_FALSE(parent.data_.owner_before(placed.data_));
            EXPECT_FALSE(placed.data_.owner_before(parent.data_));
        }
        EXPECT_TRUE(child.expired());
        EXPECT_TRUE(parent.owner_ref().managed());
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, PlacedDeviceSupportsTypedTransferAndOutlivesItsOriginalFacade) {
        std::weak_ptr<void> device_owner;
        {
            auto root = std::make_shared<DevicePair>(physical);
            root->publish(root);
            device_owner = root;
            vsl::LogicalDeviceAccessor placed{{root, &l::device_at(root->slot(0))}};
            vsl::CommandManager manager(placed);
            Native destination(placed, 4 * sizeof(std::uint32_t));
            auto result = View::FromBuffer(destination);
            std::optional<vsl::command::CopyBuffer> command;
            std::weak_ptr<void> source_owner;
            {
                Buffers buffers(placed, Buffers::Plan({4, 4}));
                source_owner = buffers.storage().owner();
                auto source = buffers.view<t::ResourceId{1}>();
                source.copy(std::array<std::uint32_t, 4>{2, 4, 6, 8});
                command.emplace(source.copy_to(result));
            }
            placed.data_.reset();
            root.reset();
            EXPECT_FALSE(device_owner.expired());
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << *command;
            command.reset();
            auto completion = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(completion.wait());
            EXPECT_TRUE(source_owner.expired());
            std::array<std::uint32_t, 4> actual{};
            result.read_into(actual);
            EXPECT_EQ(actual, (std::array<std::uint32_t, 4>{2, 4, 6, 8}));
        }
        EXPECT_TRUE(device_owner.expired());
    }
    TEST_F(DeviceStorageVulkanIntegrationTest, CancelReleasesTypedSourceWhileItsPlacedDeviceRemainsUsable) {
        auto root = std::make_shared<DevicePair>(physical);
        root->publish(root);
        const vsl::LogicalDeviceAccessor placed{{root, &l::device_at(root->slot(0))}};
        vsl::CommandManager manager(placed);
        Native destination(placed, 16);
        std::optional<vsl::command::CopyBuffer> command;
        std::weak_ptr<void> source_owner;
        {
            Buffers buffers(placed, Buffers::Plan({4, 4}));
            source_owner = buffers.storage().owner();
            command.emplace(buffers.view<t::ResourceId{1}>().copy_to(View::FromBuffer(destination)));
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << *command;
        command.reset();
        EXPECT_FALSE(source_owner.expired());
        auto completion = phase.completion();
        phase.cancel();
        EXPECT_TRUE(source_owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(placed.owner_ref().managed());
    }
} // namespace
