#pragma once
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/dependency_node.hpp>

namespace VSL_NAMESPACE::lifetime {
    // 同rootと固定依存は借用し、可変依存はbinding metadataで所有する。
    struct DescriptorOwnerContext {
        DependencyNode node;
        std::span<const OwnerRef> dependencies;
        std::vector<OwnerRef>* dependency_storage = nullptr;
        DependencyPolicy policy = DependencyPolicy::FIXED;
        bool single_resource = false;
        void normalize(std::vector<OwnerRef>& owners) const;
    };
    void visit_descriptor_edges(const impl::GraphicResourceImpl& resource, DependencyNode::EdgeVisitor visitor, void* context);
    void freeze_descriptor_dependencies(DescriptorOwnerContext& context, std::span<impl::GraphicResourceImpl* const> resources);
    void replace_descriptor_constraints(impl::GraphicResourceImpl& resource, size_t binding, std::span<const ResourceUse> uses);
    impl::GraphicResourceImpl::BindingConstraint descriptor_constraint(const ResourceUse& use, const DescriptorOwnerContext* context = nullptr);
    // 全検証とmetadataの準備を済ませてからnative更新し、要素範囲を一度にcommitする。
    void write_descriptor_binding(impl::GraphicResourceImpl& resource, vk::WriteDescriptorSet write,
                                  std::vector<impl::GraphicResourceImpl::BindingElement> elements);
    void require_descriptor_complete(const impl::GraphicResourceImpl& resource);
    void require_descriptor_valid(const impl::GraphicResourceImpl& resource);
    void require_descriptor_idle(const impl::GraphicResourceImpl& resource);
} // namespace VSL_NAMESPACE::lifetime
