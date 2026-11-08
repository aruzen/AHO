#include <VSL/define.hpp>
#include <VSL/vulkan/device.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/device_storage.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/dependency_node.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>

#include <VSL/vulkan/surface.hpp>

#include <algorithm>

namespace VSL_NAMESPACE {
    namespace {
        lifetime::OwnedRef<impl::PhysicalDeviceImpl> physical_device_owner(std::shared_ptr<impl::PhysicalDeviceImpl> data) {
            if (!data || !data.use_count() || !data->owner_node || !data->owner_node->matches(data))
                throw std::invalid_argument("Physical device requires its canonical owner");
            auto* node = data->owner_node;
            return node->ref(std::move(data));
        }
    } // namespace
} // namespace VSL_NAMESPACE

template<bool v>
std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl> VSL_NAMESPACE::LogicalDeviceGraphicCreator<v>::create(VSL_NAMESPACE::PhysicalDevice device, VSL_NAMESPACE::Surface surface) {
    (void)device.owner_ref();
    (void)surface.owner_ref();
    if (!surface.data_->surface || surface.data_->vulkan != device.data_->vulkan)
        throw std::invalid_argument("Logical device requires a surface on the physical device Instance");
    auto info = operation::graphics_device_info(*device.data_, *surface.data_, v);
    return lifetime::make_heap_device(std::move(device), std::move(info));
}

VSL_NAMESPACE::PhysicalDevice::PhysicalDevice(std::shared_ptr<impl::PhysicalDeviceImpl> data_) : data_(std::move(data_)) {}

VSL_NAMESPACE::PhysicalDevice::~PhysicalDevice() = default;
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::PhysicalDevice::owner_ref() const {
    return physical_device_owner(data_);
}
VSL_NAMESPACE::VulkanAccessor VSL_NAMESPACE::PhysicalDevice::get_instance() const {
    const auto data = physical_device_owner(data_);
    const auto parent = data->owner_context ? data->owner_context->instance.lock() : lifetime::OwnedRef<impl::VulkanImplAccessor>{};
    if (!parent.use_count() || parent.get() != data->vulkan)
        throw std::logic_error("Physical device has no Instance owner");
    VulkanAccessor instance{parent.share()};
    (void)instance.owner_ref();
    return instance;
}
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevice::Select(VulkanAccessor instance) {
    (void)instance.owner_ref();
    const auto selected = operation::select_physical_device(*instance.accessor);
    return {lifetime::make_heap_physical_device(std::move(instance), selected)};
}

uint32_t VSL_NAMESPACE::PhysicalDevice::api_version() {
    const auto data = physical_device_owner(data_);
    return operation::physical_device_properties(*data).apiVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::driver_version() {
    const auto data = physical_device_owner(data_);
    return operation::physical_device_properties(*data).driverVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::vendor_id() {
    const auto data = physical_device_owner(data_);
    return operation::physical_device_properties(*data).vendorID;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::device_id() {
    const auto data = physical_device_owner(data_);
    return operation::physical_device_properties(*data).deviceID;
}

VSL_NAMESPACE::physical_device_type::PhysicalDeviceType VSL_NAMESPACE::PhysicalDevice::type() {
    const auto data = physical_device_owner(data_);
    return physical_device_type::get_type(static_cast<int>(operation::physical_device_properties(*data).deviceType));
}

std::string VSL_NAMESPACE::PhysicalDevice::name() {
    const auto data = physical_device_owner(data_);
    return operation::physical_device_properties(*data).deviceName;
}

std::shared_ptr<VSL_NAMESPACE::impl::PhysicalDevicesImpl>
VSL_NAMESPACE::helper::enumerate_physical_devices(VulkanAccessor vulkan) {
    (void)vulkan.owner_ref();
    auto data = std::make_shared<impl::PhysicalDevicesImpl>();
    data->devices = operation::enumerate_physical_devices(*vulkan.accessor);
    data->vulkan = vulkan.accessor;
    return data;
}

std::vector<VSL_NAMESPACE::PhysicalDevice>
VSL_NAMESPACE::helper::physical_devices(const std::shared_ptr<impl::PhysicalDevicesImpl>& data) {
    const auto snapshot = data;
    if (!snapshot || !snapshot.use_count())
        throw std::invalid_argument("Physical devices snapshot is empty");
    const VulkanAccessor instance{snapshot->vulkan};
    const auto handles = snapshot->devices;
    (void)instance.owner_ref();
    std::vector<PhysicalDevice> devices;
    devices.reserve(handles.size());
    for (const auto handle : handles) {
        const auto candidate = operation::inspect_physical_device(*instance.accessor, handle);
        devices.emplace_back(lifetime::make_heap_physical_device(instance, candidate));
    }
    return devices;
}
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::helper::select_physical_device(const std::shared_ptr<impl::PhysicalDevicesImpl>& data) {
    const auto snapshot = data;
    if (!snapshot || !snapshot.use_count())
        throw std::invalid_argument("Physical devices snapshot is empty");
    const VulkanAccessor instance{snapshot->vulkan};
    const auto handles = snapshot->devices;
    (void)instance.owner_ref();
    const auto selected = operation::select_physical_device(*instance.accessor, handles);
    return {lifetime::make_heap_physical_device(instance, selected)};
}

#ifdef VSL_NAMESPACE_TEST
template<typename S>
void VSL_NAMESPACE::PhysicalDevices<S>::test() {
    std::vector<VSL_NAMESPACE::PhysicalDevice> devices;
    for (auto& d : data_->devices) {
        auto f = d.getFeatures();
        auto p = d.getProperties();
    }
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
    return operation::rate_physical_device(device.data_->device);
}

void VSL_NAMESPACE::helper::retain_physical_device_owner(
    const std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl>& logical,
    const VSL_NAMESPACE::PhysicalDevice& physical) {
    (void)physical.owner_ref();
    lifetime::require_device_owner({logical});
    if (logical->parent_device != physical.data_.get())
        throw std::invalid_argument("Logical device creator returned a different PhysicalDevice parent");
}
VSL_NAMESPACE::Surface VSL_NAMESPACE::helper::checked_surface(const std::shared_ptr<Surface>& surface) {
    if (!surface || !surface.use_count())
        throw std::invalid_argument("Logical Device requires a Surface owner");
    (void)surface->owner_ref();
    return *surface;
}

VSL_NAMESPACE::LogicalDeviceAccessor VSL_NAMESPACE::LogicalDeviceAccessor::Create(PhysicalDevice physical, DeviceCreateInfo info) {
    return {lifetime::make_heap_device(std::move(physical), std::move(info))};
}
VSL_NAMESPACE::lifetime::OwnerRef VSL_NAMESPACE::LogicalDeviceAccessor::owner_ref() const {
    lifetime::require_device_owner(*this);
    return data_->owner_node->ref(data_);
}
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::LogicalDeviceAccessor::get_physical_device() const {
    lifetime::require_device_owner(*this);
    const auto parent = data_->owner_context ? data_->owner_context->physical.lock() : lifetime::OwnedRef<impl::PhysicalDeviceImpl>{};
    if (!parent.use_count() || parent.get() != data_->parent_device)
        throw std::logic_error("Device has no PhysicalDevice owner");
    return PhysicalDevice(parent.share());
}
VSL_NAMESPACE::DeviceFeature VSL_NAMESPACE::LogicalDeviceAccessor::enabled_features() const {
    lifetime::require_device_owner(*this);
    return data_->enabled_features;
}
VSL_NAMESPACE::DeviceFeature VSL_NAMESPACE::PhysicalDevice::supported_features() const {
    const auto source = physical_device_owner(data_);
    return operation::supported_device_features(*source);
}

template struct VSL_NAMESPACE::PhysicalDevices<VSL_NAMESPACE::PhysicalDevicesSercher>;
template struct VSL_NAMESPACE::LogicalDeviceGraphicCreator<true>;
template struct VSL_NAMESPACE::LogicalDeviceGraphicCreator<false>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::LogicalDeviceGraphicCreator<true>>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::LogicalDeviceGraphicCreator<false>>;
