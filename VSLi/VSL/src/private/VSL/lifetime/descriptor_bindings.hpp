#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
    // 全検証とmetadataの準備を済ませてからnative更新し、要素範囲を一度にcommitする。
    void write_descriptor_binding(impl::GraphicResourceImpl& resource, vk::WriteDescriptorSet write,
                                  std::vector<impl::GraphicResourceImpl::BindingElement> elements);
    void require_descriptor_complete(const impl::GraphicResourceImpl& resource);
    void require_descriptor_valid(const impl::GraphicResourceImpl& resource);
    void require_descriptor_idle(const impl::GraphicResourceImpl& resource);
} // namespace VSL_NAMESPACE::lifetime
