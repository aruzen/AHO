#include <VSL/vulkan/synchronize.hpp>
#include <VSL/utils/typed_frame_resources.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

static_assert(!std::is_aggregate_v<vsl::FenceHolder>);
static_assert(!std::is_aggregate_v<vsl::SemaphoreHolder>);
static_assert(!std::is_aggregate_v<vsl::SynchroManager>);
static_assert(std::is_nothrow_move_constructible_v<vsl::FenceHolder>);
static_assert(std::is_nothrow_move_constructible_v<vsl::SemaphoreHolder>);
static_assert(std::is_nothrow_move_constructible_v<vsl::SynchroManager>);
static_assert(std::is_nothrow_swappable_v<vsl::FenceHolder>);
static_assert(std::is_nothrow_swappable_v<vsl::SemaphoreHolder>);
static_assert(std::is_nothrow_swappable_v<vsl::SynchroManager>);

void connect_synchronization(vsl::LogicalDeviceAccessor device, vsl::FenceHolder fence, vsl::SemaphoreHolder semaphore);
void connect_synchronization(vsl::LogicalDeviceAccessor device, vsl::FenceHolder fence, vsl::SemaphoreHolder semaphore) {
    vsl::SynchroManager manager(device), copied(manager);
    manager.swap(copied);
    (void)manager.get_device();
    (void)bool(manager);
    auto made = manager.create_fence("ready", true);
    fence.swap(made);
    (void)fence.owner_ref();
    (void)bool(fence);
    (void)fence.wait(std::uint64_t{0});
    (void)fence.wait(0, 0);
    auto signal = manager.create_semaphore("signal");
    semaphore.swap(signal);
    (void)semaphore.get_device();
    (void)bool(semaphore);
    vsl::typed::TypedFrameCommands commands(device, 2);
    (void)commands;
}
#if VSL_TYPED_CASE == 1
void rejected(vsl::FenceHolder value);
void rejected(vsl::FenceHolder value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 2
void rejected(vsl::FenceHolder value);
void rejected(vsl::FenceHolder value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 3
void rejected(std::shared_ptr<vsl::impl::FenceImpl> value);
void rejected(std::shared_ptr<vsl::impl::FenceImpl> value) {
    (void)vsl::FenceHolder(std::move(value));
}
#elif VSL_TYPED_CASE == 4
void rejected(vsl::lifetime::OwnedRef<vsl::impl::FenceImpl> value);
void rejected(vsl::lifetime::OwnedRef<vsl::impl::FenceImpl> value) {
    (void)vsl::FenceHolder(std::move(value));
}
#elif VSL_TYPED_CASE == 5
void rejected(vsl::SemaphoreHolder value);
void rejected(vsl::SemaphoreHolder value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 6
void rejected(vsl::SemaphoreHolder value);
void rejected(vsl::SemaphoreHolder value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 7
void rejected(std::shared_ptr<vsl::impl::SemaphoreImpl> value);
void rejected(std::shared_ptr<vsl::impl::SemaphoreImpl> value) {
    (void)vsl::SemaphoreHolder(std::move(value));
}
#elif VSL_TYPED_CASE == 8
void rejected(vsl::lifetime::OwnedRef<vsl::impl::SemaphoreImpl> value);
void rejected(vsl::lifetime::OwnedRef<vsl::impl::SemaphoreImpl> value) {
    (void)vsl::SemaphoreHolder(std::move(value));
}
#elif VSL_TYPED_CASE == 9
void rejected(vsl::SynchroManager value);
void rejected(vsl::SynchroManager value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 10
void rejected(vsl::SynchroManager value);
void rejected(vsl::SynchroManager value) {
    (void)value._registry;
}
#elif VSL_TYPED_CASE == 11
void rejected(std::shared_ptr<vsl::impl::SynchroManagerImpl> value);
void rejected(std::shared_ptr<vsl::impl::SynchroManagerImpl> value) {
    (void)vsl::SynchroManager(std::move(value));
}
#endif
