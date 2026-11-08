#pragma once
#include <VSL/operation/descriptor_ops.hpp>
#include <VSL/operation/descriptor_create_info.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
    template<typename Impl>
    struct DescriptorSlotStorage {
        alignas(Impl) std::byte value[sizeof(Impl)];
    };
    using DescriptorLayoutSlotStorage = DescriptorSlotStorage<impl::ResourceBindingLayoutImpl>;
    using DescriptorPoolSlotStorage = DescriptorSlotStorage<impl::GraphicResourcePoolImpl>;
    using DescriptorSetSlotStorage = DescriptorSlotStorage<impl::GraphicResourceImpl>;
    // 未構築の適切なsize/alignmentの領域を渡し、依存Device/Pool/Layoutを呼出側が保持する。
    void construct_descriptor_layout(void* storage, LogicalDeviceAccessor device, const DescriptorLayoutCreateInfo& info);
    void destroy_descriptor_layout(void* storage) noexcept;
    impl::ResourceBindingLayoutImpl& descriptor_layout_at(void* storage);
    void construct_descriptor_pool(void* storage, LogicalDeviceAccessor device, const DescriptorPoolCreateInfo& info);
    void destroy_descriptor_pool(void* storage) noexcept;
    impl::GraphicResourcePoolImpl& descriptor_pool_at(void* storage);
    // bulk allocation用のCPU構築。nativeが空でもdestroyできる。
    void prepare_descriptor_set(void* storage, impl::GraphicResourcePoolImpl& pool, impl::ResourceBindingLayoutImpl& layout);
    void construct_descriptor_set(void* storage, impl::GraphicResourcePoolImpl& pool, impl::ResourceBindingLayoutImpl& layout);
    // 公開前にSetのowner aliasをpool.resourcesへ弱登録する。destroyはGPU完了/取消後に一度だけ行う。
    void destroy_descriptor_set(void* storage) noexcept;
    impl::GraphicResourceImpl& descriptor_set_at(void* storage);
    std::shared_ptr<impl::ResourceBindingLayoutImpl> make_heap_descriptor_layout(LogicalDeviceAccessor device, const DescriptorLayoutCreateInfo& info);
    std::shared_ptr<impl::GraphicResourcePoolImpl> make_heap_descriptor_pool(LogicalDeviceAccessor device, const DescriptorPoolCreateInfo& info);
    using DescriptorSetAllocator = decltype(&operation::allocate_descriptor_sets);
    // allocateは成功時に全targetを初期化し、失敗時にnativeを残さない。所有・rollbackはこの層で扱う。
    std::pair<vk::Result, std::vector<std::shared_ptr<impl::GraphicResourceImpl>>> make_heap_descriptor_sets(
        const std::shared_ptr<impl::GraphicResourcePoolImpl>& pool, std::span<const std::shared_ptr<impl::ResourceBindingLayoutImpl>> layouts,
        DescriptorSetAllocator allocate = operation::allocate_descriptor_sets);
} // namespace VSL_NAMESPACE::lifetime
