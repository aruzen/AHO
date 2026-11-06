#pragma once
#include "../sync/resource_use_state.hpp"

namespace VSL_NAMESPACE {
    struct CommandBuffer;
    struct ResourceUse {
        std::shared_ptr<ResourceUseState> state;
        // stateやCompletionを含めず、使用する資源の世代を保持する。
        std::shared_ptr<void> owner;
        // Bufferのstorage状態とは別に重ねる、AHO等の論理資源の制限。
        std::shared_ptr<ResourceUseState> constraint_state;
        std::shared_ptr<void> constraint_owner;
        static ResourceUse ForBuffer(const std::shared_ptr<impl::BufferImpl>& buffer, ResourceUse constraint);
        void record(const CommandBuffer& buffer) const;
    };

    // bind_resources()以降の更新も含め、記録時点のdescriptor bindingを参照する。
    class ResourceUseGroup {
        ResourceUseState _state;
        std::shared_ptr<void> _owner;
        impl::GraphicResourceImpl* _resource = nullptr;
        std::vector<std::vector<ResourceUse>> _bindings;

    public:
        explicit ResourceUseGroup(size_t bindings, std::shared_ptr<void> owner = {});
        static std::shared_ptr<ResourceUseGroup> ForDescriptor(size_t bindings, const std::shared_ptr<impl::GraphicResourceImpl>& resource);
        bool tracks_descriptor(const impl::GraphicResourceImpl* resource) const { return _resource && _resource == resource; }
        void require_idle() const;
        void replace(size_t binding, std::vector<ResourceUse> resources);
        void record(const CommandBuffer& buffer) const;
    };
} // namespace VSL_NAMESPACE
