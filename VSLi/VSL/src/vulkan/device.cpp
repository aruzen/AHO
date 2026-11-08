#include <VSL/define.hpp>
#include <VSL/vulkan/device.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>

#include <VSL/vulkan/surface.hpp>

#include <set>
#include <cstdint>   // Necessary for UINT32_MAX
#include <algorithm> // Necessary for std::clamp

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

bool VSL_NAMESPACE::impl::helper::QueueFamilyIndices::is_complete() {
    return graphics_family.has_value() && present_family.has_value();
}

VSL_NAMESPACE::impl::helper::QueueFamilyIndices VSL_NAMESPACE::impl::helper::find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    QueueFamilyIndices indices;
    std::vector<vk::QueueFamilyProperties> queue_families = device.getQueueFamilyProperties();
    int i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = i;
        }
        vk::Bool32 present_support = device.getSurfaceSupportKHR(i, surface);
        if (present_support) {
            indices.present_family = i;
        }
        i++;
    }
    return indices;
}

using namespace VSL_NAMESPACE::impl::helper;

const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifndef _MSC_VER
    "VK_KHR_portability_subset",
#else
    "VK_KHR_shader_non_semantic_info"
#endif // _MSC_VER

    // VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
    // VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
};

bool check_device_extension_support(vk::PhysicalDevice device) {
    std::vector<vk::ExtensionProperties> available_extensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> required_extensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

template<bool v>
std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl> VSL_NAMESPACE::LogicalDeviceGraphicCreator<v>::create(VSL_NAMESPACE::PhysicalDevice device, VSL_NAMESPACE::Surface surface) {
    (void)device.owner_ref();
    (void)surface.owner_ref();
    if (!surface.data_->surface || surface.data_->vulkan != device.data_->vulkan)
        throw std::invalid_argument("Logical device requires a surface on the physical device Instance");
    std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl> data_ = std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl>(new VSL_NAMESPACE::impl::LogicalDeviceImpl);

    QueueFamilyIndices indices = find_queue_families(device.data_->device, surface.data_->surface);

    if (not check_device_extension_support(device.data_->device) || not indices.is_complete())
        throw VSL_NAMESPACE::exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices do not have requirements.");

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value()};

    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info{};
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceFeatures device_features;
    if constexpr (VALIDATION) {
        device_features.vertexPipelineStoresAndAtomics = true;
        device_features.fragmentStoresAndAtomics = true;
    }
    device_features.independentBlend = true;
    device_features.samplerAnisotropy = true;

    /* MoltenVK未対応
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
    extendedDynamicStateFeatures.extendedDynamicState = true;
     */

    vk::DeviceCreateInfo create_info;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    // createInfo.pNext = &nonSemanticFeatures;
    //createInfo.pNext = &extendedDynamicStateFeatures;

    if constexpr (v) {
        create_info.enabledLayerCount = static_cast<uint32_t>(VSL_NAMESPACE::VALIDATION_LAYERS.size());
        create_info.ppEnabledLayerNames = VSL_NAMESPACE::VALIDATION_LAYERS.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    data_->device = device.data_->device.createDevice(create_info);
    data_->graphics_family = indices.graphics_family.value();
    data_->present_family = indices.present_family.value();

    return data_;
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
    if (!logical || !logical.use_count())
        throw std::invalid_argument("Logical device creator must return an owner");
    logical->parent_device = physical.data_;
}

VSL_NAMESPACE::impl::LogicalDeviceImpl::~LogicalDeviceImpl() {
    device.destroy();
}

template struct VSL_NAMESPACE::PhysicalDevices<VSL_NAMESPACE::PhysicalDevicesSercher>;
template struct VSL_NAMESPACE::LogicalDeviceGraphicCreator<true>;
template struct VSL_NAMESPACE::LogicalDeviceGraphicCreator<false>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::LogicalDeviceGraphicCreator<true>>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::LogicalDeviceGraphicCreator<false>>;
