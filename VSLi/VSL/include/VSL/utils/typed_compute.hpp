#pragma once
#include "typed_resources.hpp"
#include "../vulkan/pipeline.hpp"
#include "../vulkan/commands/bind_graphic_resource.hpp"
#include "../vulkan/commands/dispatch.hpp"

namespace VSL_NAMESPACE::typed {
    namespace detail {
        void validate_dispatch(LogicalDeviceAccessor device, size_t x, size_t y, size_t z);
    }
    template<typename Resources>
    class TypedComputeBinding;

    template<typename Resources>
    class TypedDispatch final : public command::Command, public command::PipelineHolder {
        friend class TypedComputeBinding<Resources>;
        VSL_NAMESPACE::ComputePipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::vector<command::BindGraphicResource> _resources;
        command::Dispatch _dispatch;
        TypedDispatch(VSL_NAMESPACE::ComputePipeline pipeline, LogicalDeviceAccessor device, std::vector<command::BindGraphicResource> resources,
                      std::uint32_t x, std::uint32_t y, std::uint32_t z)
            : _pipeline(std::move(pipeline)), _device(device), _resources(std::move(resources)), _dispatch(x, y, z) {}

    public:
        using ResourceLayout = Resources;
        PipelineAccessor get_pipeline() override { return _pipeline; }
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
    template<typename Resources = ResourceSchema<>>
    class TypedComputePipeline;

    template<typename Resources>
    class TypedComputeBinding {
        friend class TypedComputePipeline<Resources>;
        VSL_NAMESPACE::ComputePipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::vector<command::BindGraphicResource> _resources;
        TypedComputeBinding(VSL_NAMESPACE::ComputePipeline pipeline, LogicalDeviceAccessor device, std::vector<command::BindGraphicResource> resources)
            : _pipeline(std::move(pipeline)), _device(device), _resources(std::move(resources)) {}

    public:
        using ResourceLayout = Resources;
        TypedDispatch<Resources> dispatch(size_t x, size_t y = 1, size_t z = 1) const;
    };

    template<typename Resources>
    class TypedComputePipeline {
        LogicalDeviceAccessor _device;
        VSL_NAMESPACE::ComputePipeline _pipeline;
        template<pipeline_layout_injecter... Stages>
        static VSL_NAMESPACE::ComputePipeline MakePipeline(LogicalDeviceAccessor device, Stages&&... stages);

    public:
        using ResourceLayout = Resources;
        template<pipeline_layout_injecter... Stages>
        TypedComputePipeline(LogicalDeviceAccessor device, Stages&&... stages)
            : _device(device), _pipeline(MakePipeline(device, std::forward<Stages>(stages)...)) {}
        const VSL_NAMESPACE::ComputePipeline& backend() const { return _pipeline; }
        template<typename... Sets>
        requires compatible_descriptor_sets<Resources, Sets...>
        TypedComputeBinding<Resources> bind_resources(const Sets&... sets) const;
    };

    template<typename Resources>
    void TypedDispatch<Resources>::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (_device.data_ != manager.get_device().data_)
            throw std::invalid_argument("Typed compute pipeline/command manager device mismatch");
        _pipeline.invoke(pool, buffer, manager);
        for (auto& resource : _resources)
            resource.invoke(pool, buffer, manager);
        _dispatch.invoke(pool, buffer, manager);
    }
    template<typename Resources>
    TypedDispatch<Resources> TypedComputeBinding<Resources>::dispatch(size_t x, size_t y, size_t z) const {
        detail::validate_dispatch(_device, x, y, z);
        return TypedDispatch<Resources>(_pipeline, _device, _resources, std::uint32_t(x), std::uint32_t(y), std::uint32_t(z));
    }
    template<typename Resources>
    template<pipeline_layout_injecter... Stages>
    VSL_NAMESPACE::ComputePipeline TypedComputePipeline<Resources>::MakePipeline(LogicalDeviceAccessor device, Stages&&... stages) {
        PipelineLayout layout(device, std::forward<Stages>(stages)..., Resources{});
        detail::validate_resource_schema(layout, Resources::REQUIREMENTS);
        return VSL_NAMESPACE::ComputePipeline(std::move(layout).freeze());
    }
    template<typename Resources>
    template<typename... Sets>
    requires compatible_descriptor_sets<Resources, Sets...>
    TypedComputeBinding<Resources> TypedComputePipeline<Resources>::bind_resources(const Sets&... sets) const {
        if (((sets.device().data_ != _device.data_) || ...))
            throw std::invalid_argument("Typed descriptor/compute pipeline device mismatch");
        (sets.require_complete(), ...);
        std::vector<command::BindGraphicResource> resources;
        resources.reserve(sizeof...(Sets));
        size_t set_index = 0;
        (resources.emplace_back(sets.backend(), graphic_resource::BindingDestination::COMPUTE, _pipeline,
                                Resources::SET_NUMBERS[set_index++]),
         ...);
        return TypedComputeBinding<Resources>(_pipeline, _device, std::move(resources));
    }
} // namespace VSL_NAMESPACE::typed
