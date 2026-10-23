#pragma once
#include "typed_resources.hpp"
#include "typed_push_constants.hpp"
#include "../vulkan/pipeline.hpp"
#include "../vulkan/commands/bind_graphic_resource.hpp"
#include "../vulkan/commands/dispatch.hpp"

namespace VSL_NAMESPACE::typed {
    template<typename...>
    class TypedBatch;
    namespace detail {
        void validate_dispatch(LogicalDeviceAccessor device, size_t x, size_t y, size_t z);
    }
    template<typename Resources, typename Pushes, bool pushed = Pushes::EMPTY>
    class TypedComputeBinding;

    template<typename Resources, typename Pushes>
    class TypedDispatch final : public command::Command, public command::PipelineHolder {
        template<typename...>
        friend class TypedBatch;
        template<typename, typename, bool>
        friend class TypedComputeBinding;
        VSL_NAMESPACE::ComputePipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::vector<command::BindGraphicResource> _resources;
        std::optional<typename Pushes::Values> _pushes;
        command::Dispatch _dispatch;
        TypedDispatch(VSL_NAMESPACE::ComputePipeline pipeline, LogicalDeviceAccessor device, std::vector<command::BindGraphicResource> resources,
                      std::uint32_t x, std::uint32_t y, std::uint32_t z, std::optional<typename Pushes::Values> pushes)
            : _pipeline(std::move(pipeline)), _device(device), _resources(std::move(resources)), _pushes(std::move(pushes)), _dispatch(x, y, z) {}

    public:
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        PipelineAccessor get_pipeline() override { return _pipeline; }
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
    template<typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>>
    class TypedComputePipeline;

    template<typename Resources, typename Pushes, bool pushed>
    class TypedComputeBinding {
        template<typename, typename, bool>
        friend class TypedComputeBinding;
        friend class TypedComputePipeline<Resources, Pushes>;
        VSL_NAMESPACE::ComputePipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::vector<command::BindGraphicResource> _resources;
        std::optional<typename Pushes::Values> _pushes;
        TypedComputeBinding(VSL_NAMESPACE::ComputePipeline pipeline, LogicalDeviceAccessor device, std::vector<command::BindGraphicResource> resources, std::optional<typename Pushes::Values> pushes = std::nullopt)
            : _pipeline(std::move(pipeline)), _device(device), _resources(std::move(resources)), _pushes(std::move(pushes)) {}

    public:
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        TypedDispatch<Resources, Pushes> dispatch(size_t x, size_t y = 1, size_t z = 1) const
        requires(pushed);
        TypedComputeBinding<Resources, Pushes, true> push_constants(const typename Pushes::Values& values) const;
    };

    template<typename Resources, typename Pushes>
    class TypedComputePipeline {
        LogicalDeviceAccessor _device;
        VSL_NAMESPACE::ComputePipeline _pipeline;
        template<pipeline_layout_injecter... Stages>
        static VSL_NAMESPACE::ComputePipeline MakePipeline(LogicalDeviceAccessor device, Stages&&... stages);

    public:
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        template<pipeline_layout_injecter... Stages>
        TypedComputePipeline(LogicalDeviceAccessor device, Stages&&... stages)
            : _device(device), _pipeline(MakePipeline(device, std::forward<Stages>(stages)...)) {}
        const VSL_NAMESPACE::ComputePipeline& backend() const { return _pipeline; }
        template<typename... Sets>
        requires compatible_descriptor_sets<Resources, Sets...>
        TypedComputeBinding<Resources, Pushes> bind_resources(const Sets&... sets) const;
    };

    template<typename Resources, typename Pushes>
    void TypedDispatch<Resources, Pushes>::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (_device.data_ != manager.get_device().data_)
            throw std::invalid_argument("Typed compute pipeline/command manager device mismatch");
        _pipeline.invoke(pool, buffer, manager);
        for (auto& resource : _resources)
            resource.invoke(pool, buffer, manager);
        if constexpr (!Pushes::EMPTY)
            detail::invoke_push_values(_pipeline, buffer, Pushes::REQUIREMENTS, _pushes->bytes());
        _dispatch.invoke(pool, buffer, manager);
    }
    template<typename Resources, typename Pushes, bool pushed>
    TypedDispatch<Resources, Pushes> TypedComputeBinding<Resources, Pushes, pushed>::dispatch(size_t x, size_t y, size_t z) const
    requires(pushed)
    {
        detail::validate_dispatch(_device, x, y, z);
        return TypedDispatch<Resources, Pushes>(_pipeline, _device, _resources, std::uint32_t(x), std::uint32_t(y), std::uint32_t(z), _pushes);
    }
    template<typename Resources, typename Pushes>
    template<pipeline_layout_injecter... Stages>
    VSL_NAMESPACE::ComputePipeline TypedComputePipeline<Resources, Pushes>::MakePipeline(LogicalDeviceAccessor device, Stages&&... stages) {
        PipelineLayout layout(device, std::forward<Stages>(stages)..., Resources{}, Pushes{});
        detail::validate_resource_schema(layout, Resources::REQUIREMENTS);
        detail::validate_push_schema(layout, Pushes::REQUIREMENTS);
        return VSL_NAMESPACE::ComputePipeline(std::move(layout).freeze());
    }
    template<typename Resources, typename Pushes>
    template<typename... Sets>
    requires compatible_descriptor_sets<Resources, Sets...>
    TypedComputeBinding<Resources, Pushes> TypedComputePipeline<Resources, Pushes>::bind_resources(const Sets&... sets) const {
        if (((sets.device().data_ != _device.data_) || ...))
            throw std::invalid_argument("Typed descriptor/compute pipeline device mismatch");
        (sets.require_complete(), ...);
        std::vector<command::BindGraphicResource> resources;
        resources.reserve(sizeof...(Sets));
        size_t set_index = 0;
        (resources.emplace_back(sets.backend(), graphic_resource::BindingDestination::COMPUTE, _pipeline,
                                Resources::SET_NUMBERS[set_index++]),
         ...);
        return TypedComputeBinding<Resources, Pushes>(_pipeline, _device, std::move(resources));
    }
    template<typename Resources, typename Pushes, bool pushed>
    TypedComputeBinding<Resources, Pushes, true> TypedComputeBinding<Resources, Pushes, pushed>::push_constants(const typename Pushes::Values& values) const {
        return TypedComputeBinding<Resources, Pushes, true>(_pipeline, _device, _resources, std::optional<typename Pushes::Values>(values));
    }
} // namespace VSL_NAMESPACE::typed
