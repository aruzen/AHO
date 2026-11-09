#pragma once
#include "../sync/resource_use_state.hpp"
#include "owner_ref.hpp"

namespace VSL_NAMESPACE {
    enum class ResourceRetention {
        SUBMISSION,
        COMPLETION
    };
    struct CommandBuffer;
    struct BufferAccessor;
    struct ImageAccessor;
    struct SamplerAccessor;
    class BufferView;
    namespace graphic_resource {
        struct Resource;
    }
    namespace command {
        struct BindGraphicResource;
    }
    struct ResourceUse {
        lifetime::OwnedRef<ResourceUseState> state;
        // stateやCompletionを含めず、使用する資源の世代を保持する。
        lifetime::OwnerRef owner;
        // 物理storage状態とは別に重ねる、AHO等の論理資源の制限。
        lifetime::OwnedRef<ResourceUseState> constraint_state;
        lifetime::OwnerRef constraint_owner;
        static ResourceUse ForBuffer(const BufferAccessor& buffer, ResourceUse constraint = {});
        static ResourceUse ForBuffer(const BufferView& buffer, ResourceUse constraint = {});
        static ResourceUse ForImage(const ImageAccessor& image);
        static ResourceUse ForSampler(const SamplerAccessor& sampler);
        ResourceUse with_constraint(ResourceUse constraint) const;
        void record(const CommandBuffer& buffer, ResourceRetention retention = ResourceRetention::SUBMISSION) const;
    };

    // bind_resources()以降の更新も含め、記録時点のdescriptor bindingを参照する。
    class ResourceUseGroup {
        friend struct command::BindGraphicResource;
        ResourceUseState _state;
        std::shared_ptr<void> _owner;
        impl::GraphicResourceImpl* _resource = nullptr;
        ResourceRetention _retention = ResourceRetention::SUBMISSION;
        std::vector<std::vector<ResourceUse>> _bindings;
        void retain_descriptor_owner(const graphic_resource::Resource& resource, const CommandBuffer& buffer) const;

    public:
        explicit ResourceUseGroup(size_t bindings, std::shared_ptr<void> owner = {});
        static std::shared_ptr<ResourceUseGroup> ForDescriptor(size_t bindings, const graphic_resource::Resource& resource, ResourceRetention retention = ResourceRetention::SUBMISSION);
        bool tracks_descriptor(const graphic_resource::Resource& resource) const;
        void require_idle() const;
        void replace(size_t binding, std::vector<ResourceUse> resources);
        void record(const CommandBuffer& buffer) const;
    };
} // namespace VSL_NAMESPACE
