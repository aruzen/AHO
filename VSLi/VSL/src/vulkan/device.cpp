#include <VSL/define.hpp>
#include <VSL/vulkan/device.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>

#include <VSL/vulkan/surface.hpp>

#include <set>
#include <cstdint>   // Necessary for UINT32_MAX
#include <algorithm> // Necessary for std::clamp

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

void VSL_NAMESPACE::impl::PhysicalDeviceImpl::make_props() {
    props = device.getProperties();
}

void VSL_NAMESPACE::impl::PhysicalDeviceImpl::make_mem_props() {
    mem_props = device.getMemoryProperties();
}

VSL_NAMESPACE::PhysicalDevice::PhysicalDevice(std::shared_ptr<impl::PhysicalDeviceImpl> data_) : data_(data_) {}

VSL_NAMESPACE::PhysicalDevice::~PhysicalDevice() {}

uint32_t VSL_NAMESPACE::PhysicalDevice::api_version() {
    if (!data_->props.has_value())
        data_->make_props();
    return data_->props.value().apiVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::driver_version() {
    if (!data_->props.has_value())
        data_->make_props();
    return data_->props.value().driverVersion;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::vendor_id() {
    if (!data_->props.has_value())
        data_->make_props();
    return data_->props.value().vendorID;
}

uint32_t VSL_NAMESPACE::PhysicalDevice::device_id() {
    if (!data_->props.has_value())
        data_->make_props();
    return data_->props.value().deviceID;
}

VSL_NAMESPACE::physical_device_type::PhysicalDeviceType VSL_NAMESPACE::PhysicalDevice::type() {
    if (!data_->props.has_value())
        data_->make_props();
    return VSL_NAMESPACE::physical_device_type::get_type((size_t)data_->props.value().deviceType);
}

std::string VSL_NAMESPACE::PhysicalDevice::name() {
    if (!data_->props.has_value())
        data_->make_props();
    return data_->props.value().deviceName;
}

std::shared_ptr<VSL_NAMESPACE::impl::PhysicalDevicesImpl>
VSL_NAMESPACE::helper::enumerate_physical_devices(VulkanAccessor vulkan) {
    auto data = std::make_shared<impl::PhysicalDevicesImpl>();
    data->devices = vulkan.accessor->instance.enumeratePhysicalDevices();
    data->vulkan = vulkan.accessor;
    return data;
}

std::vector<VSL_NAMESPACE::PhysicalDevice>
VSL_NAMESPACE::helper::physical_devices(const std::shared_ptr<impl::PhysicalDevicesImpl>& data) {
    std::vector<PhysicalDevice> devices;
    devices.reserve(data->devices.size());
    for (auto handle : data->devices) {
        auto device = std::make_shared<impl::PhysicalDeviceImpl>();
        device->device = handle;
        device->vulkan = data->vulkan;
        devices.emplace_back(std::move(device));
    }
    return devices;
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

int rate_device_suitability(vk::PhysicalDevice device) {
    auto device_properties = device.getProperties();
    auto device_features = device.getFeatures();

    int score = 0;

    if (device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    score += device_properties.limits.maxImageDimension2D;

    if (!device_features.geometryShader) {
        return 0;
    }
    return score;
}

int VSL_NAMESPACE::PhysicalDevicesSercher::benchmark(VSL_NAMESPACE::PhysicalDevice device) {
    return rate_device_suitability(device.data_->device);
}

void VSL_NAMESPACE::helper::retain_physical_device_owner(
    const std::shared_ptr<VSL_NAMESPACE::impl::LogicalDeviceImpl>& logical,
    const VSL_NAMESPACE::PhysicalDevice& physical) {
    logical->parent_device = physical.data_;
}

VSL_NAMESPACE::impl::LogicalDeviceImpl::~LogicalDeviceImpl() {
    device.destroy();
}

template struct VSL_NAMESPACE::PhysicalDevices<VSL_NAMESPACE::PhysicalDevicesSercher>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::LogicalDeviceGraphicCreator<true>>;
template struct VSL_NAMESPACE::LogicalDevice<VSL_NAMESPACE::LogicalDeviceGraphicCreator<false>>;
