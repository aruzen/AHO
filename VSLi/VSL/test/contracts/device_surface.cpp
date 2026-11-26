#include <VSL/lifetime/startup_arena.hpp>
#include <type_traits>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

#if VSL_TYPED_CASE == 0
static_assert(!std::is_aggregate_v<vsl::LogicalDeviceAccessor>);
static_assert(!std::is_aggregate_v<vsl::Surface>);
static_assert(!std::is_base_of_v<vsl::LogicalDeviceAccessor, vsl::LogicalDevice<>>);
static_assert(std::is_nothrow_move_assignable_v<vsl::LogicalDevice<>>);
static_assert(std::is_nothrow_move_constructible_v<vsl::Surface>);
static_assert(noexcept(vsl::WeakLogicalDevice{}.lock()));
static_assert(noexcept(vsl::WeakSurface{}.lock()));

struct Creator {
    vsl::LogicalDeviceAccessor create(vsl::PhysicalDevice physical, vsl::Surface surface) {
        return vsl::LogicalDeviceAccessor::CreateGraphics(std::move(physical), std::move(surface), false);
    }
};

void connect_device(vsl::PhysicalDevice physical, vsl::Surface surface, vsl::lifetime::StartupArena& arena);

void connect_device(vsl::PhysicalDevice physical, vsl::Surface surface, vsl::lifetime::StartupArena& arena) {
    auto info = vsl::LogicalDeviceAccessor::GraphicsInfo(physical, surface, false);
    auto device = vsl::LogicalDeviceAccessor::Create(physical, std::move(info));
    vsl::LogicalDevice<Creator> custom(physical, surface);
    vsl::LogicalDeviceAccessor copy = custom;
    vsl::LogicalDevice<> reused(copy);
    auto weak = device.weak();
    auto locked = weak.lock();
    device.swap(locked);
    device = arena.device({1});
    surface = arena.surface({2});
    auto surface_weak = surface.weak();
    auto saved = surface_weak.lock();
    saved.swap(surface);
    (void)reused.get_physical_device();
    (void)reused.enabled_features();
    (void)device.owner_ref();
    (void)surface.get_instance();
    (void)surface.get_window_owner();
    (void)weak.expired();
    weak.reset();
    surface_weak.reset();
}
#elif VSL_TYPED_CASE == 1
void reject(vsl::LogicalDeviceAccessor value);

void reject(vsl::LogicalDeviceAccessor value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 2
void reject(vsl::Surface value);

void reject(vsl::Surface value) {
    (void)value.data_;
}
#elif VSL_TYPED_CASE == 3
void reject(std::shared_ptr<vsl::impl::LogicalDeviceImpl> value);

void reject(std::shared_ptr<vsl::impl::LogicalDeviceImpl> value) {
    (void)vsl::LogicalDeviceAccessor(value);
}
#elif VSL_TYPED_CASE == 4
void reject(std::shared_ptr<vsl::impl::SurfaceImpl> value);

void reject(std::shared_ptr<vsl::impl::SurfaceImpl> value) {
    (void)vsl::Surface(value);
}
#elif VSL_TYPED_CASE == 5
void reject(vsl::lifetime::OwnedRef<vsl::impl::LogicalDeviceImpl> value);

void reject(vsl::lifetime::OwnedRef<vsl::impl::LogicalDeviceImpl> value) {
    (void)vsl::LogicalDeviceAccessor(value);
}
#elif VSL_TYPED_CASE == 6
void reject(vsl::lifetime::OwnedRef<vsl::impl::SurfaceImpl> value);

void reject(vsl::lifetime::OwnedRef<vsl::impl::SurfaceImpl> value) {
    (void)vsl::Surface(value);
}
#elif VSL_TYPED_CASE == 7
void reject(vsl::LogicalDevice<>& value);

void reject(vsl::LogicalDevice<>& value) {
    vsl::LogicalDeviceAccessor& base = value;
    (void)base;
}
#elif VSL_TYPED_CASE == 8
void reject(vsl::LogicalDeviceAccessor value);

void reject(vsl::LogicalDeviceAccessor value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 9
void reject(vsl::Surface value);

void reject(vsl::Surface value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 10
void reject(vsl::LogicalDevice<>& value);

void reject(vsl::LogicalDevice<>& value) {
    (void)value._value;
}
#elif VSL_TYPED_CASE == 11
void reject(vsl::WeakLogicalDevice value);

void reject(vsl::WeakLogicalDevice value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 12
void reject(vsl::WeakSurface value);

void reject(vsl::WeakSurface value) {
    (void)value._owner;
}
#elif VSL_TYPED_CASE == 13
void reject(std::weak_ptr<vsl::impl::LogicalDeviceImpl> value);

void reject(std::weak_ptr<vsl::impl::LogicalDeviceImpl> value) {
    (void)vsl::WeakLogicalDevice(value);
}
#elif VSL_TYPED_CASE == 14
void reject(std::weak_ptr<vsl::impl::SurfaceImpl> value);

void reject(std::weak_ptr<vsl::impl::SurfaceImpl> value) {
    (void)vsl::WeakSurface(value);
}
#elif VSL_TYPED_CASE == 15
struct MutatingCreator {
    vsl::LogicalDeviceAccessor create(vsl::PhysicalDevice&, vsl::Surface);
};

void reject(vsl::PhysicalDevice physical, vsl::Surface surface);

void reject(vsl::PhysicalDevice physical, vsl::Surface surface) {
    (void)vsl::LogicalDevice<MutatingCreator>(physical, surface);
}
#endif
