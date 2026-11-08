#include <VSL/operation/device_ops.hpp>
#include <VSL/operation/physical_device_ops.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::operation {
    namespace {
        constexpr std::array<VkBool32 VkPhysicalDeviceFeatures::*, 55> CORE_FEATURE_FIELDS{
            &VkPhysicalDeviceFeatures::robustBufferAccess,
            &VkPhysicalDeviceFeatures::fullDrawIndexUint32,
            &VkPhysicalDeviceFeatures::imageCubeArray,
            &VkPhysicalDeviceFeatures::independentBlend,
            &VkPhysicalDeviceFeatures::geometryShader,
            &VkPhysicalDeviceFeatures::tessellationShader,
            &VkPhysicalDeviceFeatures::sampleRateShading,
            &VkPhysicalDeviceFeatures::dualSrcBlend,
            &VkPhysicalDeviceFeatures::logicOp,
            &VkPhysicalDeviceFeatures::multiDrawIndirect,
            &VkPhysicalDeviceFeatures::drawIndirectFirstInstance,
            &VkPhysicalDeviceFeatures::depthClamp,
            &VkPhysicalDeviceFeatures::depthBiasClamp,
            &VkPhysicalDeviceFeatures::fillModeNonSolid,
            &VkPhysicalDeviceFeatures::depthBounds,
            &VkPhysicalDeviceFeatures::wideLines,
            &VkPhysicalDeviceFeatures::largePoints,
            &VkPhysicalDeviceFeatures::alphaToOne,
            &VkPhysicalDeviceFeatures::multiViewport,
            &VkPhysicalDeviceFeatures::samplerAnisotropy,
            &VkPhysicalDeviceFeatures::textureCompressionETC2,
            &VkPhysicalDeviceFeatures::textureCompressionASTC_LDR,
            &VkPhysicalDeviceFeatures::textureCompressionBC,
            &VkPhysicalDeviceFeatures::occlusionQueryPrecise,
            &VkPhysicalDeviceFeatures::pipelineStatisticsQuery,
            &VkPhysicalDeviceFeatures::vertexPipelineStoresAndAtomics,
            &VkPhysicalDeviceFeatures::fragmentStoresAndAtomics,
            &VkPhysicalDeviceFeatures::shaderTessellationAndGeometryPointSize,
            &VkPhysicalDeviceFeatures::shaderImageGatherExtended,
            &VkPhysicalDeviceFeatures::shaderStorageImageExtendedFormats,
            &VkPhysicalDeviceFeatures::shaderStorageImageMultisample,
            &VkPhysicalDeviceFeatures::shaderStorageImageReadWithoutFormat,
            &VkPhysicalDeviceFeatures::shaderStorageImageWriteWithoutFormat,
            &VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing,
            &VkPhysicalDeviceFeatures::shaderSampledImageArrayDynamicIndexing,
            &VkPhysicalDeviceFeatures::shaderStorageBufferArrayDynamicIndexing,
            &VkPhysicalDeviceFeatures::shaderStorageImageArrayDynamicIndexing,
            &VkPhysicalDeviceFeatures::shaderClipDistance,
            &VkPhysicalDeviceFeatures::shaderCullDistance,
            &VkPhysicalDeviceFeatures::shaderFloat64,
            &VkPhysicalDeviceFeatures::shaderInt64,
            &VkPhysicalDeviceFeatures::shaderInt16,
            &VkPhysicalDeviceFeatures::shaderResourceResidency,
            &VkPhysicalDeviceFeatures::shaderResourceMinLod,
            &VkPhysicalDeviceFeatures::sparseBinding,
            &VkPhysicalDeviceFeatures::sparseResidencyBuffer,
            &VkPhysicalDeviceFeatures::sparseResidencyImage2D,
            &VkPhysicalDeviceFeatures::sparseResidencyImage3D,
            &VkPhysicalDeviceFeatures::sparseResidency2Samples,
            &VkPhysicalDeviceFeatures::sparseResidency4Samples,
            &VkPhysicalDeviceFeatures::sparseResidency8Samples,
            &VkPhysicalDeviceFeatures::sparseResidency16Samples,
            &VkPhysicalDeviceFeatures::sparseResidencyAliased,
            &VkPhysicalDeviceFeatures::variableMultisampleRate,
            &VkPhysicalDeviceFeatures::inheritedQueries};
        static_assert(static_cast<std::uint64_t>(DeviceFeature::INHERITED_QUERIES) == (1ull << (CORE_FEATURE_FIELDS.size() - 1)));
        constexpr std::uint64_t KNOWN_FEATURES = (static_cast<std::uint64_t>(DeviceFeature::TIMELINE_SEMAPHORE) << 1) - 1;
        bool has_extension(std::span<const vk::ExtensionProperties> extensions, std::string_view name);
        std::uint32_t effective_api(impl::PhysicalDeviceImpl& physical);
        DeviceFeature query_features(impl::PhysicalDeviceImpl& physical, std::span<const vk::ExtensionProperties> extensions);
        bool has_extension(std::span<const vk::ExtensionProperties> extensions, std::string_view name) {
            return std::ranges::any_of(extensions, [&](const auto& extension) { return name == extension.extensionName.data(); });
        }
        std::uint32_t effective_api(impl::PhysicalDeviceImpl& physical) {
            return std::min(physical.vulkan->api_version, physical_device_properties(physical).apiVersion);
        }
        DeviceFeature query_features(impl::PhysicalDeviceImpl& physical, std::span<const vk::ExtensionProperties> extensions) {
            if (!physical.device || !physical.vulkan || !physical.vulkan->instance)
                throw std::invalid_argument("Device features require a live PhysicalDevice and Instance");
            if (!physical.features)
                physical.features = physical.device.getFeatures();
            const auto& native = static_cast<const VkPhysicalDeviceFeatures&>(*physical.features);
            std::uint64_t mask = 0;
            for (size_t i = 0; i < CORE_FEATURE_FIELDS.size(); ++i)
                if (native.*CORE_FEATURE_FIELDS[i])
                    mask |= std::uint64_t{1} << i;
            const auto& instance = *physical.vulkan;
            const bool core_query = instance.api_version >= VK_API_VERSION_1_1;
            const bool timeline_available = effective_api(physical) >= VK_API_VERSION_1_2 || has_extension(extensions, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
            if (timeline_available && (core_query || instance.physical_device_properties2)) {
                const auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(vkGetInstanceProcAddr(instance.instance,
                                                                                                            core_query ? "vkGetPhysicalDeviceFeatures2" : "vkGetPhysicalDeviceFeatures2KHR"));
                if (!query)
                    throw std::runtime_error("Enabled PhysicalDevice features2 entry point is unavailable");
                VkPhysicalDeviceTimelineSemaphoreFeatures timeline{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
                VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &timeline};
                query(physical.device, &features);
                if (timeline.timelineSemaphore)
                    mask |= static_cast<std::uint64_t>(DeviceFeature::TIMELINE_SEMAPHORE);
            }
            return static_cast<DeviceFeature>(mask);
        }
    } // namespace
    DeviceFeature supported_device_features(impl::PhysicalDeviceImpl& physical) {
        if (!physical.device)
            throw std::invalid_argument("Device features require a live PhysicalDevice");
        const auto extensions = physical.device.enumerateDeviceExtensionProperties();
        return query_features(physical, extensions);
    }
    DeviceCreateInfo graphics_device_info(impl::PhysicalDeviceImpl& physical, impl::SurfaceImpl& surface, bool validation) {
        if (!physical.device || !physical.vulkan || surface.vulkan != physical.vulkan || !surface.surface)
            throw std::invalid_argument("Graphics Device requires a Surface on its PhysicalDevice Instance");
        DeviceCreateInfo info;
        const auto families = physical.device.getQueueFamilyProperties();
        for (size_t i = 0; i < families.size(); ++i) {
            if (!families[i].queueCount)
                continue;
            if (families[i].queueFlags & vk::QueueFlagBits::eGraphics)
                info.graphics_family = static_cast<std::uint32_t>(i);
            if (physical.device.getSurfaceSupportKHR(static_cast<std::uint32_t>(i), surface.surface))
                info.present_family = static_cast<std::uint32_t>(i);
        }
        if (!info.graphics_family || !info.present_family)
            throw std::invalid_argument("Graphics Device requires graphics and presentation queues");
        info.require_compute = false;
        info.extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef _MSC_VER
        info.extensions.emplace_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif
        info.features = DeviceFeature::SAMPLER_ANISOTROPY | DeviceFeature::INDEPENDENT_BLEND;
        if (validation)
            info.features = info.features | DeviceFeature::VERTEX_PIPELINE_STORES_AND_ATOMICS | DeviceFeature::FRAGMENT_STORES_AND_ATOMICS;
        return info;
    }
    void initialize_device(impl::LogicalDeviceImpl& target, impl::PhysicalDeviceImpl& physical, DeviceCreateInfo info, const DeviceNativeCalls& calls) {
        if (target.device || target.parent_device)
            throw std::logic_error("Device is already initialized");
        if (!physical.device || !physical.vulkan || !physical.vulkan->instance)
            throw std::invalid_argument("Device requires a live PhysicalDevice and Instance");
        if (static_cast<std::uint64_t>(info.features) & ~KNOWN_FEATURES)
            throw std::invalid_argument("Unknown Device feature bits");
        const auto families = physical.device.getQueueFamilyProperties();
        const auto required = vk::QueueFlagBits::eGraphics | (info.require_compute ? vk::QueueFlagBits::eCompute : vk::QueueFlags{});
        if (!info.graphics_family) {
            for (size_t i = 0; i < families.size(); ++i)
                if (families[i].queueCount && (families[i].queueFlags & required) == required &&
                    (info.queues.empty() || std::ranges::any_of(info.queues, [&](const auto& request) { return request.family == i; }))) {
                    info.graphics_family = static_cast<std::uint32_t>(i);
                    break;
                }
        }
        if (!info.graphics_family || *info.graphics_family >= families.size() || !families[*info.graphics_family].queueCount ||
            (families[*info.graphics_family].queueFlags & required) != required)
            throw std::invalid_argument("Device requires a valid graphics queue family");
        const auto graphics = *info.graphics_family, present = info.present_family.value_or(graphics);
        if (present >= families.size() || !families[present].queueCount)
            throw std::invalid_argument("Device presentation queue family is invalid");
        if (info.queues.empty()) {
            info.queues.push_back({graphics});
            if (present != graphics)
                info.queues.push_back({present});
        }
        if (info.queues.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Device queue family count exceeds uint32");
        std::vector<VkDeviceQueueCreateInfo> queues;
        queues.reserve(info.queues.size());
        for (const auto& request : info.queues) {
            if (request.family >= families.size() || request.priorities.empty() || request.priorities.size() > families[request.family].queueCount ||
                std::ranges::any_of(queues, [&](const auto& known) { return known.queueFamilyIndex == request.family; }) ||
                std::ranges::any_of(request.priorities, [](float priority) { return !std::isfinite(priority) || priority < 0.0f || priority > 1.0f; }))
                throw std::invalid_argument("Device queue family/count/priorities are invalid");
            queues.push_back({.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = request.family, .queueCount = static_cast<std::uint32_t>(request.priorities.size()), .pQueuePriorities = request.priorities.data()});
        }
        for (const auto role : {graphics, present})
            if (!std::ranges::any_of(queues, [&](const auto& queue) { return queue.queueFamilyIndex == role; }))
                throw std::invalid_argument("Device role requires a created queue");
        const auto extensions = physical.device.enumerateDeviceExtensionProperties();
        const auto supported = query_features(physical, extensions);
        if (!has_device_features(supported, info.features))
            throw std::invalid_argument("Requested Device feature is unsupported or lacks its Instance query capability");
        if (has_extension(extensions, "VK_KHR_portability_subset"))
            info.extensions.emplace_back("VK_KHR_portability_subset");
        const bool timeline_enabled = has_device_features(info.features, DeviceFeature::TIMELINE_SEMAPHORE);
        if (timeline_enabled && effective_api(physical) < VK_API_VERSION_1_2)
            info.extensions.emplace_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        std::vector<const char*> names;
        names.reserve(info.extensions.size());
        for (const auto& name : info.extensions) {
            if (name.empty() || name.find('\0') != std::string::npos || !has_extension(extensions, name))
                throw std::invalid_argument("Device extension is invalid or unsupported: " + name);
            if (!std::ranges::any_of(names, [&](const char* existing) { return name == existing; }))
                names.push_back(name.c_str());
        }
        if (names.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Device extension count exceeds uint32");
        VkPhysicalDeviceFeatures features{};
        for (size_t i = 0; i < CORE_FEATURE_FIELDS.size(); ++i)
            features.*CORE_FEATURE_FIELDS[i] = (static_cast<std::uint64_t>(info.features) >> i) & 1u;
        const VkPhysicalDeviceTimelineSemaphoreFeatures timeline{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, .timelineSemaphore = timeline_enabled};
        const VkDeviceCreateInfo native{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = timeline_enabled ? &timeline : nullptr, .queueCreateInfoCount = static_cast<std::uint32_t>(queues.size()), .pQueueCreateInfos = queues.data(), .enabledExtensionCount = static_cast<std::uint32_t>(names.size()), .ppEnabledExtensionNames = names.data(), .pEnabledFeatures = &features};
        VkDevice handle = VK_NULL_HANDLE;
        const auto result = calls.create ? calls.create(calls.context, physical.device, &native, &handle)
                                         : vkCreateDevice(physical.device, &native, nullptr, &handle);
        if (result != VK_SUCCESS)
            vk::detail::throwResultException(static_cast<vk::Result>(result), "Device creation failed");
        if (!handle)
            throw std::runtime_error("Device creation returned an empty handle");
        target.device = handle;
        target.parent_device = &physical;
        target.graphics_family = graphics;
        target.present_family = present;
        target.enabled_features = info.features;
        target.swapchain_enabled = std::ranges::find(info.extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME) != info.extensions.end();
    }
    void destroy_device(impl::LogicalDeviceImpl& target, const DeviceNativeCalls& calls) noexcept {
        const auto native = std::exchange(target.device, {});
        if (!native)
            return;
        if (calls.destroy)
            calls.destroy(calls.context, native);
        else
            vkDestroyDevice(native, nullptr);
        target.enabled_features = DeviceFeature::NONE;
        target.swapchain_enabled = false;
    }
} // namespace VSL_NAMESPACE::operation

VSL_NAMESPACE::impl::LogicalDeviceImpl::~LogicalDeviceImpl() {
    operation::destroy_device(*this);
}
