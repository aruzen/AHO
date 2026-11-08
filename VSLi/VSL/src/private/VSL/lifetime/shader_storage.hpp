#pragma once
#include <VSL/operation/shader_ops.hpp>

namespace VSL_NAMESPACE::lifetime {
    struct ShaderSlotStorage {
        alignas(impl::ShaderImpl) std::byte shader[sizeof(impl::ShaderImpl)];
    };
    // 呼出側は未構築のsizeof/alignof(ShaderSlotStorage)領域とDevice所有を破棄まで維持する。
    void construct_shader(void* storage, LogicalDeviceAccessor device, ShaderCreateInfo info);
    void construct_shader(void* storage, impl::LogicalDeviceImpl& device, ShaderCreateInfo info);
    void destroy_shader(void* storage) noexcept;
    impl::ShaderImpl& shader_at(void* storage);
    std::shared_ptr<impl::ShaderImpl> make_heap_shader(LogicalDeviceAccessor device, ShaderCreateInfo info);
} // namespace VSL_NAMESPACE::lifetime
