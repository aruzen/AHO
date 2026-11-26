#pragma once
#include <VSL/define.hpp>
#include <vulkan/vulkan_core.h>

namespace VSL_NAMESPACE::sync {
    // Native結果のcommit前は同Deviceの返却submit／回収を延期する。所有や完了証明は持たない。
    class NativeDeviceCall {
        static thread_local const NativeDeviceCall* CURRENT;
        VkDevice _device;
        const NativeDeviceCall* _previous;
        bool _active = true;

    public:
        explicit NativeDeviceCall(VkDevice device) noexcept;
        ~NativeDeviceCall();
        void finish() noexcept;
        NativeDeviceCall(const NativeDeviceCall&) = delete;
        NativeDeviceCall& operator=(const NativeDeviceCall&) = delete;
        static bool Active(VkDevice device) noexcept;
    };
} // namespace VSL_NAMESPACE::sync
