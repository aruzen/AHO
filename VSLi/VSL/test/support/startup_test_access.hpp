#pragma once
#include <VSL/lifetime/startup_access.hpp>

namespace vsl_test {
    // 不正owner拒否の試験専用。通常生成・importには使用しない。
    inline vsl::VulkanAccessor unchecked_startup(std::shared_ptr<vsl::impl::VulkanImplAccessor> source) {
        vsl::VulkanAccessor value;
        vsl::lifetime::detail::StartupAccess::Owned(value) = std::move(source);
        return value;
    }
    inline vsl::PhysicalDevice unchecked_startup(std::shared_ptr<vsl::impl::PhysicalDeviceImpl> source) {
        vsl::PhysicalDevice value;
        vsl::lifetime::detail::StartupAccess::Owned(value) = std::move(source);
        return value;
    }
} // namespace vsl_test
