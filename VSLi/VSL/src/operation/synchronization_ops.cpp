#include <VSL/operation/synchronization_ops.hpp>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    void initialize_semaphore(impl::SemaphoreImpl& semaphore, impl::LogicalDeviceImpl& device, SemaphoreCreateInfo info) {
        if (!semaphore.semaphores.empty())
            throw std::logic_error("Semaphore is already initialized");
        semaphore.device = &device;
        semaphore.native_device = device.device;
        semaphore.semaphores = create_semaphores(device.device, info.count);
    }
    void initialize_fence(impl::FenceImpl& fence, impl::LogicalDeviceImpl& device, FenceCreateInfo info) {
        if (!fence.fences.empty())
            throw std::logic_error("Fence is already initialized");
        validate_synchronization_count(device.device, info.count);
        fence.device = &device;
        fence.native_device = device.device;
        fence.uses.resize(info.count);
        fence.fences = create_fences(device.device, info.count, info.signaled);
    }
    void validate_synchronization_count(vk::Device device, size_t count) {
        if (!device || !count || count > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Synchronization array requires a device and a positive uint32 count");
    }
    std::vector<vk::Semaphore> create_semaphores(vk::Device device, size_t count) {
        validate_synchronization_count(device, count);
        std::vector<vk::Semaphore> handles(count);
        try {
            for (auto& handle : handles)
                handle = device.createSemaphore({});
        } catch (...) {
            destroy_semaphores(device, handles);
            throw;
        }
        return handles;
    }
    std::vector<vk::Fence> create_fences(vk::Device device, size_t count, bool signaled) {
        validate_synchronization_count(device, count);
        std::vector<vk::Fence> handles(count);
        const vk::FenceCreateInfo info{.flags = signaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlags{}};
        try {
            for (auto& handle : handles)
                handle = device.createFence(info);
        } catch (...) {
            destroy_fences(device, handles);
            throw;
        }
        return handles;
    }
    void destroy_semaphores(vk::Device device, std::span<const vk::Semaphore> semaphores) noexcept {
        for (auto it = semaphores.rbegin(); it != semaphores.rend(); ++it)
            if (*it)
                device.destroySemaphore(*it);
    }
    void destroy_fences(vk::Device device, std::span<const vk::Fence> fences) noexcept {
        for (auto it = fences.rbegin(); it != fences.rend(); ++it)
            if (*it)
                device.destroyFence(*it);
    }
    vk::Result wait_fences(vk::Device device, std::span<const vk::Fence> fences, std::uint64_t timeout) noexcept {
        return static_cast<vk::Result>(VULKAN_HPP_DEFAULT_DISPATCHER.vkWaitForFences(static_cast<VkDevice>(device),
                                                                                     static_cast<std::uint32_t>(fences.size()), reinterpret_cast<const VkFence*>(fences.data()), VK_TRUE, timeout));
    }
    void reset_fences(vk::Device device, std::span<const vk::Fence> fences) {
        device.resetFences(vk::ArrayProxy<const vk::Fence>(static_cast<std::uint32_t>(fences.size()), fences.data()));
    }
} // namespace VSL_NAMESPACE::operation
