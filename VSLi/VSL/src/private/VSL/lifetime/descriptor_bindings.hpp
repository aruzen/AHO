#pragma once
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::lifetime {
    // arena内部はselfと構築時に固定した外部依存だけを借用する。
    struct DescriptorOwnerContext {
        std::weak_ptr<void> self;
        const std::vector<std::shared_ptr<void>>* dependencies = nullptr;
        void normalize(std::vector<std::shared_ptr<void>>& owners) const;
    };
    impl::GraphicResourceImpl::BindingConstraint descriptor_constraint(const ResourceUse& use, const DescriptorOwnerContext* context = nullptr);
    // 全検証とmetadataの準備を済ませてからnative更新し、要素範囲を一度にcommitする。
    void write_descriptor_binding(impl::GraphicResourceImpl& resource, vk::WriteDescriptorSet write,
                                  std::vector<impl::GraphicResourceImpl::BindingElement> elements);
    void require_descriptor_complete(const impl::GraphicResourceImpl& resource);
    void require_descriptor_valid(const impl::GraphicResourceImpl& resource);
    void require_descriptor_idle(const impl::GraphicResourceImpl& resource);
} // namespace VSL_NAMESPACE::lifetime
