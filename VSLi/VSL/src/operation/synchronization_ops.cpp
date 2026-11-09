#include <VSL/operation/synchronization_ops.hpp>
#include <limits>
#include <stdexcept>

namespace VSL_NAMESPACE::operation {
    namespace {
        thread_local const SynchronizationNativeCalls* NATIVE_CALLS = nullptr;
    }
    ScopedSynchronizationNativeCalls::ScopedSynchronizationNativeCalls(SynchronizationNativeCalls calls) noexcept
        : _calls(calls), _previous(NATIVE_CALLS) {
        NATIVE_CALLS = &_calls;
    }
    ScopedSynchronizationNativeCalls::~ScopedSynchronizationNativeCalls() {
        NATIVE_CALLS = _previous;
    }
    void initialize_semaphore(impl::SemaphoreImpl& semaphore, impl::LogicalDeviceImpl& device, SemaphoreCreateInfo info) {
        if (!semaphore.semaphores.empty())
            throw std::logic_error("Semaphore is already initialized");
        validate_synchronization_count(device.device, info.count);
        semaphore.use.poisoned.resize(info.count, false);
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
            const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0};
            for (auto& handle : handles) {
                VkSemaphore native = VK_NULL_HANDLE;
                const auto result = static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->create_semaphore
                                                                ? NATIVE_CALLS->create_semaphore(NATIVE_CALLS->context, device, &info, &native)
                                                                : VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateSemaphore(device, &info, nullptr, &native));
                if (result != vk::Result::eSuccess)
                    vk::detail::throwResultException(result, "Semaphore creation failed");
                if (!native)
                    throw std::runtime_error("Semaphore creation returned a null handle");
                handle = native;
            }
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
            if (*it) {
                if (NATIVE_CALLS && NATIVE_CALLS->destroy_semaphore)
                    NATIVE_CALLS->destroy_semaphore(NATIVE_CALLS->context, device, *it);
                else
                    device.destroySemaphore(*it);
            }
    }
    void destroy_fences(vk::Device device, std::span<const vk::Fence> fences) noexcept {
        for (auto it = fences.rbegin(); it != fences.rend(); ++it)
            if (*it)
                device.destroyFence(*it);
    }
    void reset_fences(vk::Device device, std::span<const vk::Fence> fences) {
        const auto count = static_cast<std::uint32_t>(fences.size());
        const auto* handles = reinterpret_cast<const VkFence*>(fences.data());
        const auto result = static_cast<vk::Result>(NATIVE_CALLS && NATIVE_CALLS->reset_fences
                                                        ? NATIVE_CALLS->reset_fences(NATIVE_CALLS->context, device, count, handles)
                                                        : VULKAN_HPP_DEFAULT_DISPATCHER.vkResetFences(device, count, handles));
        if (result != vk::Result::eSuccess)
            vk::detail::throwResultException(result, "Fence reset failed");
    }
} // namespace VSL_NAMESPACE::operation
