#pragma once
#include <VSL/operation/instance_ops.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct InstanceSlotStorage {
        alignas(impl::VulkanImplAccessor) std::byte value[sizeof(impl::VulkanImplAccessor)];
    };
    void construct_instance(void* storage, InstanceCreateInfo info, const operation::InstanceNativeCalls& calls = {});
    void destroy_instance(void* storage) noexcept;
    impl::VulkanImplAccessor& instance_at(void* storage);
    std::shared_ptr<impl::VulkanImplAccessor> make_heap_instance(InstanceCreateInfo info, const operation::InstanceNativeCalls& calls = {});
} // namespace VSL_NAMESPACE::lifetime
