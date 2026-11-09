#pragma once
#include "../../src/window_impl.hpp"
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/synchronization_ops.hpp>
#include <VSL/operation/swapchain_ops.hpp>
#include <GLFW/glfw3.h>
#include <functional>
#include <utility>

namespace aho_test {
    struct FrameBufferQuery {
        bool zero = false;
        unsigned calls = 0;
        unsigned callback_at = 1;
        std::function<void()> after_read;
        aho::window::detail::ScopedFrameBufferQuery scope;
        FrameBufferQuery() : scope({this, Read}) {}
        static void Read(void* context, void* native, int* width, int* height);
    };
    struct AcquisitionCalls {
        unsigned acquisitions = 0, resets = 0, creates = 0;
        std::function<void()> after_reset;
        vsl::operation::ScopedPresentationNativeCalls presentation;
        vsl::operation::ScopedSynchronizationNativeCalls synchronization;
        vsl::operation::ScopedSwapchainNativeCalls swapchain;
        AcquisitionCalls();
        static VkResult Acquire(void* context, VkDevice device, VkSwapchainKHR chain, std::uint64_t timeout,
                                VkSemaphore semaphore, VkFence fence, std::uint32_t* image) noexcept;
        static VkResult Reset(void* context, VkDevice device, std::uint32_t count, const VkFence* fences) noexcept;
        static VkResult Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output);
    };
    inline void FrameBufferQuery::Read(void* context, void* native, int* width, int* height) {
        auto& source = *static_cast<FrameBufferQuery*>(context);
        ++source.calls;
        if (source.zero)
            *width = *height = 0;
        else
            glfwGetFramebufferSize(static_cast<GLFWwindow*>(native), width, height);
        if (source.calls == source.callback_at)
            if (auto callback = std::exchange(source.after_read, {}))
                callback();
    }
    inline AcquisitionCalls::AcquisitionCalls()
        : presentation({.context = this, .acquire = Acquire}),
          synchronization({.context = this, .reset_fences = Reset}), swapchain({this, Create}) {}
    inline VkResult AcquisitionCalls::Acquire(void* context, VkDevice device, VkSwapchainKHR chain, std::uint64_t timeout,
                                              VkSemaphore semaphore, VkFence fence, std::uint32_t* image) noexcept {
        ++static_cast<AcquisitionCalls*>(context)->acquisitions;
        return vkAcquireNextImageKHR(device, chain, timeout, semaphore, fence, image);
    }
    inline VkResult AcquisitionCalls::Reset(void* context, VkDevice device, std::uint32_t count, const VkFence* fences) noexcept {
        auto& source = *static_cast<AcquisitionCalls*>(context);
        ++source.resets;
        const auto result = vkResetFences(device, count, fences);
        if (auto callback = std::exchange(source.after_reset, {}))
            callback();
        return result;
    }
    inline VkResult AcquisitionCalls::Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output) {
        ++static_cast<AcquisitionCalls*>(context)->creates;
        return vkCreateSwapchainKHR(device, info, nullptr, output);
    }
} // namespace aho_test
