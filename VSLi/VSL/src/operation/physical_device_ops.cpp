#include <VSL/operation/physical_device_ops.hpp>
#include <VSL/exceptions.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        PhysicalDeviceCandidate inspect_candidate(impl::VulkanImplAccessor& instance, vk::PhysicalDevice device, std::span<const vk::PhysicalDevice> available);
        PhysicalDeviceCandidate select_candidate(impl::VulkanImplAccessor& instance, std::span<const vk::PhysicalDevice> candidates, std::span<const vk::PhysicalDevice> available);
        PhysicalDeviceCandidate inspect_candidate(impl::VulkanImplAccessor& instance, vk::PhysicalDevice device, std::span<const vk::PhysicalDevice> available) {
            if (!device || std::ranges::find(available, device) == available.end())
                throw std::invalid_argument("Physical device does not belong to its Instance");
            return {&instance, device, device.getProperties(), device.getFeatures()};
        }
        PhysicalDeviceCandidate select_candidate(impl::VulkanImplAccessor& instance, std::span<const vk::PhysicalDevice> candidates, std::span<const vk::PhysicalDevice> available) {
            int best = -1;
            PhysicalDeviceCandidate selected;
            for (const auto native : candidates) {
                auto candidate = inspect_candidate(instance, native, available);
                const auto score = score_physical_device(candidate.properties, candidate.features);
                if (score > best) {
                    best = score;
                    selected = std::move(candidate);
                }
            }
            if (!selected.device)
                throw exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices is not suitable.");
            return selected;
        }
    } // namespace
    std::vector<vk::PhysicalDevice> enumerate_physical_devices(impl::VulkanImplAccessor& instance) {
        if (!instance.instance)
            throw std::invalid_argument("Physical devices require a live Instance");
        return instance.instance.enumeratePhysicalDevices();
    }
    PhysicalDeviceCandidate inspect_physical_device(impl::VulkanImplAccessor& instance, vk::PhysicalDevice device) {
        const auto available = enumerate_physical_devices(instance);
        return inspect_candidate(instance, device, available);
    }
    int score_physical_device(const vk::PhysicalDeviceProperties& properties, const vk::PhysicalDeviceFeatures& features) {
        if (!features.geometryShader)
            return 0;
        const auto score = std::uint64_t{properties.limits.maxImageDimension2D} + (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1000 : 0);
        return static_cast<int>(std::min(score, std::uint64_t{std::numeric_limits<int>::max()}));
    }
    int rate_physical_device(vk::PhysicalDevice device) {
        if (!device)
            throw std::invalid_argument("Physical device is empty");
        return score_physical_device(device.getProperties(), device.getFeatures());
    }
    PhysicalDeviceCandidate select_physical_device(impl::VulkanImplAccessor& instance, std::span<const vk::PhysicalDevice> candidates) {
        const auto available = enumerate_physical_devices(instance);
        return select_candidate(instance, candidates, available);
    }
    PhysicalDeviceCandidate select_physical_device(impl::VulkanImplAccessor& instance) {
        const auto available = enumerate_physical_devices(instance);
        return select_candidate(instance, available, available);
    }
    void initialize_physical_device(impl::PhysicalDeviceImpl& target, impl::VulkanImplAccessor& instance, const PhysicalDeviceCandidate& candidate) {
        if (target.device || target.vulkan)
            throw std::logic_error("Physical device is already initialized");
        if (!instance.instance || candidate.instance != &instance || !candidate.device)
            throw std::invalid_argument("Physical device candidate requires its original Instance");
        target.vulkan = &instance;
        target.device = candidate.device;
        target.props = candidate.properties;
        target.features = candidate.features;
    }
    vk::PhysicalDeviceProperties query_physical_device_properties(vk::PhysicalDevice device) {
        if (!device)
            throw std::invalid_argument("Physical device is empty");
        return device.getProperties();
    }
    vk::PhysicalDeviceMemoryProperties query_physical_device_memory(vk::PhysicalDevice device) {
        if (!device)
            throw std::invalid_argument("Physical device is empty");
        return device.getMemoryProperties();
    }
    const vk::PhysicalDeviceProperties& physical_device_properties(impl::PhysicalDeviceImpl& device) {
        if (!device.props)
            device.props = query_physical_device_properties(device.device);
        return *device.props;
    }
} // namespace VSL_NAMESPACE::operation

void VSL_NAMESPACE::impl::PhysicalDeviceImpl::make_props() {
    props = operation::query_physical_device_properties(device);
}
void VSL_NAMESPACE::impl::PhysicalDeviceImpl::make_mem_props() {
    mem_props = operation::query_physical_device_memory(device);
}
