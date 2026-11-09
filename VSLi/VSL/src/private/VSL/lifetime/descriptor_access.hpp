#pragma once
#include <VSL/vulkan/descriptor_set_manager.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct DescriptorAccess {
        static const OwnedRef<impl::ResourceBindingLayoutImpl>& Owned(const graphic_resource::BindingLayout& value) { return value._owner; }
        static OwnedRef<impl::ResourceBindingLayoutImpl>& Owned(graphic_resource::BindingLayout& value) { return value._owner; }
        static OwnedRef<impl::ResourceBindingLayoutImpl> Owned(graphic_resource::BindingLayout&& value) { return std::move(value._owner); }
        static OwnedRef<impl::ResourceBindingLayoutImpl> Owned(const graphic_resource::BindingLayout&& value) { return value._owner; }
        static graphic_resource::BindingLayout FromOwned(OwnedRef<impl::ResourceBindingLayoutImpl> owner) { return graphic_resource::BindingLayout(std::move(owner)); }
        static graphic_resource::BindingLayout FromShared(std::shared_ptr<impl::ResourceBindingLayoutImpl> owner);

        static const OwnedRef<impl::GraphicResourcePoolImpl>& Owned(const graphic_resource::Pool& value) { return value._owner; }
        static OwnedRef<impl::GraphicResourcePoolImpl>& Owned(graphic_resource::Pool& value) { return value._owner; }
        static OwnedRef<impl::GraphicResourcePoolImpl> Owned(graphic_resource::Pool&& value) { return std::move(value._owner); }
        static OwnedRef<impl::GraphicResourcePoolImpl> Owned(const graphic_resource::Pool&& value) { return value._owner; }
        static graphic_resource::Pool FromOwned(OwnedRef<impl::GraphicResourcePoolImpl> owner) { return graphic_resource::Pool(std::move(owner)); }
        static graphic_resource::Pool FromShared(std::shared_ptr<impl::GraphicResourcePoolImpl> owner);

        static const OwnedRef<impl::GraphicResourceImpl>& Owned(const graphic_resource::Resource& value) { return value._owner; }
        static OwnedRef<impl::GraphicResourceImpl>& Owned(graphic_resource::Resource& value) { return value._owner; }
        static OwnedRef<impl::GraphicResourceImpl> Owned(graphic_resource::Resource&& value) { return std::move(value._owner); }
        static OwnedRef<impl::GraphicResourceImpl> Owned(const graphic_resource::Resource&& value) { return value._owner; }
        static graphic_resource::Resource FromOwned(OwnedRef<impl::GraphicResourceImpl> owner) { return graphic_resource::Resource(std::move(owner)); }
        static graphic_resource::Resource FromShared(std::shared_ptr<impl::GraphicResourceImpl> owner);
    };
    struct DescriptorManagerContext {
        std::shared_ptr<impl::GraphicResourceManagerImpl> state;
        std::shared_ptr<impl::GraphicResourceManagerIdentity> identity;
    };
    struct DescriptorManagerAccess {
        static const std::shared_ptr<impl::GraphicResourceManagerImpl>& Data(const graphic_resource::ManagerInterface& manager) { return manager._state; }
        static std::shared_ptr<impl::GraphicResourceManagerImpl>& Data(graphic_resource::ManagerInterface& manager) { return manager._state; }
        static std::shared_ptr<impl::GraphicResourceManagerImpl> Data(graphic_resource::ManagerInterface&& manager) { return std::move(manager._state); }
        static std::shared_ptr<impl::GraphicResourceManagerImpl> Data(const graphic_resource::ManagerInterface&& manager) { return manager._state; }
        static DescriptorManagerContext Snapshot(const graphic_resource::ManagerInterface& manager) { return {manager._state, manager._identity}; }
    };
} // namespace VSL_NAMESPACE::lifetime::detail
