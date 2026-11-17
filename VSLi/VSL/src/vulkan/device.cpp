#include <VSL/define.hpp>
#include <VSL/vulkan/device.hpp>
#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/device_storage.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/dependency_node.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>

#include <VSL/vulkan/surface.hpp>

#include <algorithm>

namespace VSL_NAMESPACE {
    using lifetime::detail::StartupAccess;
    namespace {
        lifetime::OwnedRef<impl::PhysicalDeviceImpl> physical_device_owner(const PhysicalDevice& device);
        DeviceCreateInfo graphics_info(const PhysicalDevice& physical, const Surface& surface, bool validation, PresentFencePolicy present_fences);
        lifetime::OwnedRef<impl::PhysicalDeviceImpl> physical_device_owner(const PhysicalDevice& device) {
            const auto& owner = StartupAccess::Owned(device);
            StartupAccess::RequireCanonical(owner);
            return owner;
        }
        DeviceCreateInfo graphics_info(const PhysicalDevice& physical, const Surface& surface, bool validation, PresentFencePolicy present_fences) {
            StartupAccess::RequireCanonical(StartupAccess::Owned(physical));
            StartupAccess::RequireCanonical(StartupAccess::Owned(surface));
            if (StartupAccess::Owned(surface)->vulkan != StartupAccess::Owned(physical)->vulkan)
                throw std::invalid_argument("Logical device requires a surface on the physical device Instance");
            return operation::graphics_device_info(*StartupAccess::Owned(physical), *StartupAccess::Owned(surface), validation, present_fences);
        }
    } // namespace
} // namespace VSL_NAMESPACE

VSL_NAMESPACE::PhysicalDevice::~PhysicalDevice() = default;
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::PhysicalDevice::owner_ref() const {
    return physical_device_owner(*this);
}
VSL_NAMESPACE::VulkanAccessor VSL_NAMESPACE::PhysicalDevice::get_instance() const {
    const auto data = physical_device_owner(*this);
    const auto parent = data->owner_context ? data->owner_context->instance.lock() : lifetime::OwnedRef<impl::VulkanImplAccessor>{};
    if (!parent.use_count() || parent.get() != data->vulkan)
        throw std::logic_error("Physical device has no Instance owner");
    return StartupAccess::FromOwned(parent);
}
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevice::Select(VulkanAccessor instance) {
    StartupAccess::RequireCanonical(StartupAccess::Owned(instance));
    const auto selected = operation::select_physical_device(*StartupAccess::Owned(instance));
    return StartupAccess::FromShared(lifetime::make_heap_physical_device(std::move(instance), selected));
}

uint32_t VSL_NAMESPACE::PhysicalDevice::api_version() {
    const auto data = physical_device_owner(*this);
    return operation::physical_device_properties(*data).apiVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::driver_version() {
    const auto data = physical_device_owner(*this);
    return operation::physical_device_properties(*data).driverVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::vendor_id() {
    const auto data = physical_device_owner(*this);
    return operation::physical_device_properties(*data).vendorID;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::device_id() {
    const auto data = physical_device_owner(*this);
    return operation::physical_device_properties(*data).deviceID;
}

VSL_NAMESPACE::physical_device_type::PhysicalDeviceType VSL_NAMESPACE::PhysicalDevice::type() {
    const auto data = physical_device_owner(*this);
    return physical_device_type::get_type(static_cast<int>(operation::physical_device_properties(*data).deviceType));
}

std::string VSL_NAMESPACE::PhysicalDevice::name() {
    const auto data = physical_device_owner(*this);
    return operation::physical_device_properties(*data).deviceName;
}

#ifdef VSL_NAMESPACE_TEST
template<typename S>
void VSL_NAMESPACE::PhysicalDevices<S>::test() {
    for (auto d : get()) {
        VSL_NAMESPACE::loggingln(d.name());
    }
}
#endif // VSL_NAMESPACE_TEST

VSL_NAMESPACE::physical_device_type::PhysicalDeviceType VSL_NAMESPACE::physical_device_type::get_type(int a) {
    switch (a) {
        case 1:
            return VSL_NAMESPACE::physical_device_type::INTEGRATED_GPU;
        case 2:
            return VSL_NAMESPACE::physical_device_type::DISCRETE_GPU;
        case 3:
            return VSL_NAMESPACE::physical_device_type::VIRTUAL_GPU;
        case 4:
            return VSL_NAMESPACE::physical_device_type::CPU;
        default:
            return VSL_NAMESPACE::physical_device_type::OTHER;
    }
}

int VSL_NAMESPACE::PhysicalDevicesSercher::benchmark(VSL_NAMESPACE::PhysicalDevice device) {
    (void)device.owner_ref();
    return operation::rate_physical_device(StartupAccess::Owned(device)->device);
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::LogicalDeviceAccessor::CheckRuntime(LogicalDeviceAccessor value) {
    StartupAccess::RequireCanonical(StartupAccess::Owned(value));
    return value;
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::LogicalDeviceAccessor::CheckCreatorResult(const PhysicalDevice& physical, LogicalDeviceAccessor value) {
    StartupAccess::RequireCanonical(StartupAccess::Owned(physical));
    StartupAccess::RequireCanonical(StartupAccess::Owned(value));
    if (StartupAccess::Owned(value)->parent_device != StartupAccess::Owned(physical).get())
        throw std::invalid_argument("Logical device creator returned a different PhysicalDevice parent");
    return value;
}
VSL_NAMESPACE::Surface VSL_NAMESPACE::LogicalDeviceAccessor::ReadSurface(std::shared_ptr<Surface> surface) {
    if (!surface || !surface.use_count())
        throw std::invalid_argument("Logical Device requires a Surface owner");
    StartupAccess::RequireCanonical(StartupAccess::Owned(*surface));
    return *surface;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::LogicalDeviceAccessor::Create(PhysicalDevice physical, DeviceCreateInfo info) {
    return StartupAccess::FromShared(lifetime::make_heap_device(std::move(physical), std::move(info)));
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::LogicalDeviceAccessor::owner_ref() const {
    StartupAccess::RequireCanonical(_owner);
    return _owner;
}
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::LogicalDeviceAccessor::get_physical_device() const {
    const auto owner = _owner;
    StartupAccess::RequireCanonical(owner);
    const auto parent = owner->owner_context ? owner->owner_context->physical.lock() : lifetime::OwnedRef<impl::PhysicalDeviceImpl>{};
    if (!parent.use_count() || parent.get() != owner->parent_device)
        throw std::logic_error("Device has no PhysicalDevice owner");
    return StartupAccess::FromOwned(parent);
}
VSL_NAMESPACE::DeviceFeature VSL_NAMESPACE::LogicalDeviceAccessor::enabled_features() const {
    StartupAccess::RequireCanonical(_owner);
    return _owner->enabled_features;
}
VSL_NAMESPACE::DeviceCreateInfo VSL_NAMESPACE::LogicalDeviceAccessor::GraphicsInfo(PhysicalDevice physical, Surface surface, bool validation, PresentFencePolicy present_fences) {
    return graphics_info(physical, surface, validation, present_fences);
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::LogicalDeviceAccessor::CreateGraphics(PhysicalDevice physical, Surface surface, bool validation, PresentFencePolicy present_fences) {
    auto info = graphics_info(physical, surface, validation, present_fences);
    return Create(std::move(physical), std::move(info));
}
VSL_NAMESPACE::WeakLogicalDevice VSL_NAMESPACE::LogicalDeviceAccessor::weak() const noexcept {
    return WeakLogicalDevice(_owner.weak());
}
VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::WeakLogicalDevice::lock() const noexcept {
    return LogicalDeviceAccessor(_owner.lock());
}
VSL_NAMESPACE::DeviceFeature VSL_NAMESPACE::PhysicalDevice::supported_features() const {
    const auto source = physical_device_owner(*this);
    return operation::supported_device_features(*source);
}

template struct VSL_NAMESPACE::PhysicalDevices<VSL_NAMESPACE::PhysicalDevicesSercher>;
template struct VSL_NAMESPACE::GraphicsDeviceCreator<true>;
template struct VSL_NAMESPACE::GraphicsDeviceCreator<false>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::GraphicsDeviceCreator<true>>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::GraphicsDeviceCreator<false>>;
