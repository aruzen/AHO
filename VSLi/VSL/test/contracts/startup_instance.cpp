#include <VSL/lifetime/startup_arena.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

#if VSL_TYPED_CASE == 0
static_assert(!std::is_aggregate_v<vsl::VulkanAccessor>);
static_assert(!std::is_aggregate_v<vsl::PhysicalDevice>);
static_assert(std::is_nothrow_move_constructible_v<vsl::Vulkan<false>>);
static_assert(std::is_nothrow_move_assignable_v<vsl::PhysicalDevice>);
static_assert(!std::is_convertible_v<vsl::Vulkan<false>&, vsl::VulkanAccessor&>);
struct Searcher {
    int benchmark(vsl::PhysicalDevice device) const { return device.api_version() ? 1 : -1; }
};
void connect_instance(vsl::InstanceCreateInfo info);
void connect_instance(vsl::InstanceCreateInfo info) {
    auto instance = vsl::VulkanAccessor::Create(info);
    auto physical = vsl::PhysicalDevice::Select(instance);
    vsl::Vulkan<false> facade(instance);
    vsl::VulkanAccessor copied = facade;
    auto listed = facade.devices<Searcher>();
    auto selected = listed.search();
    auto candidates = listed.get();
    auto moved = std::move(physical);
    physical = selected;
    physical.swap(moved);
    (void)physical.get_instance();
    (void)copied.owner_ref();
    constexpr std::array<vsl::storage::StartupRequest, 2> requests{{{{1}, vsl::storage::StartupKind::INSTANCE}, {{2}, vsl::storage::StartupKind::PHYSICAL_DEVICE}}};
    constexpr vsl::storage::StartupPlan<2> plan(requests);
    const std::array<vsl::lifetime::StartupInit, 2> inputs{info, vsl::lifetime::PhysicalDeviceInit{vsl::typed::ResourceId{1}, {}}};
    vsl::lifetime::StartupArena arena(plan, inputs);
    auto arena_instance = arena.instance({1});
    auto arena_physical = arena.physical_device({2});
    (void)arena_physical.get_instance();
    (void)arena_instance.owner_ref();
    (void)candidates;
}
#elif VSL_TYPED_CASE == 1
void reject(vsl::VulkanAccessor value);
void reject(vsl::VulkanAccessor value) {
    (void)value.accessor;
}
#elif VSL_TYPED_CASE == 2
void reject(vsl::Vulkan<false>& value);
void reject(vsl::Vulkan<false>& value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 3
void reject(vsl::Vulkan<false>& value);
void reject(vsl::Vulkan<false>& value) {
    (void)value.accessor;
}
#elif VSL_TYPED_CASE == 4
void reject(vsl::PhysicalDevice value);
void reject(vsl::PhysicalDevice value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 5
void reject(vsl::PhysicalDevices<>& value);
void reject(vsl::PhysicalDevices<>& value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 6
void reject(std::shared_ptr<vsl::impl::VulkanImplAccessor> value);
void reject(std::shared_ptr<vsl::impl::VulkanImplAccessor> value) {
    (void)vsl::VulkanAccessor(value);
}
#elif VSL_TYPED_CASE == 7
void reject(std::shared_ptr<vsl::impl::PhysicalDeviceImpl> value);
void reject(std::shared_ptr<vsl::impl::PhysicalDeviceImpl> value) {
    (void)vsl::PhysicalDevice(value);
}
#elif VSL_TYPED_CASE == 8
void reject(vsl::lifetime::OwnedRef<vsl::impl::VulkanImplAccessor> value);
void reject(vsl::lifetime::OwnedRef<vsl::impl::VulkanImplAccessor> value) {
    (void)vsl::VulkanAccessor(value);
}
#elif VSL_TYPED_CASE == 9
void reject(vsl::lifetime::OwnedRef<vsl::impl::PhysicalDeviceImpl> value);
void reject(vsl::lifetime::OwnedRef<vsl::impl::PhysicalDeviceImpl> value) {
    (void)vsl::PhysicalDevice(value);
}
#elif VSL_TYPED_CASE == 10
void reject(vsl::VulkanAccessor value);
void reject(vsl::VulkanAccessor value) {
    (void)vsl::helper::enumerate_physical_devices(value);
}
#elif VSL_TYPED_CASE == 11
void reject(std::shared_ptr<vsl::impl::PhysicalDevicesImpl> value);
void reject(std::shared_ptr<vsl::impl::PhysicalDevicesImpl> value) {
    (void)vsl::helper::physical_devices(value);
}
#elif VSL_TYPED_CASE == 12
void reject(std::shared_ptr<vsl::impl::PhysicalDevicesImpl> value);
void reject(std::shared_ptr<vsl::impl::PhysicalDevicesImpl> value) {
    (void)vsl::helper::select_physical_device(value);
}
#elif VSL_TYPED_CASE == 13
void reject(vsl::Vulkan<false>& value);
void reject(vsl::Vulkan<false>& value) {
    vsl::VulkanAccessor& borrowed = value;
    (void)borrowed;
}
#elif VSL_TYPED_CASE == 14
void reject(vsl::VulkanAccessor value);
void reject(vsl::VulkanAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 15
void reject(vsl::PhysicalDevice value);
void reject(vsl::PhysicalDevice value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 16
void reject(vsl::PhysicalDevices<>& value);
void reject(vsl::PhysicalDevices<>& value) {
    (void)value._collection;
}
#elif VSL_TYPED_CASE == 17
void reject(vsl::Vulkan<false>& value);
void reject(vsl::Vulkan<false>& value) {
    (void)value._value;
}
#endif
