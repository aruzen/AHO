#pragma once
#include <VSL/vulkan/device.hpp>
#include <VSL/lifetime/dependency_node.hpp>

namespace VSL_NAMESPACE::lifetime {
    void require_device_owner(const LogicalDeviceAccessor& device);
    LogicalDeviceAccessor device_from_impl(impl::LogicalDeviceImpl& device);
    template<typename Owner>
    LogicalDeviceAccessor device_from_child(const Owner& child) {
        if (!child || !child.use_count() || !child->device || (child->owner_node && child->owner_node->retiring()))
            throw std::invalid_argument("Device getter requires a live owning child");
        return device_from_impl(*child->device);
    }
} // namespace VSL_NAMESPACE::lifetime
