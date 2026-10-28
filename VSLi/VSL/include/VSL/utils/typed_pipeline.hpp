#pragma once
#include "typed_shader.hpp"
#include "typed_index_buffer.hpp"
#include "typed_vertex_buffer.hpp"
#include "../vulkan/commands/draw_indexed.hpp"
#include "typed_resources.hpp"
#include "typed_push_constants.hpp"
#include "../vulkan/commands/bind_graphic_resource.hpp"
#include "../vulkan/pipeline.hpp"
#include "../vulkan/commands/bind_vertex_buffers.hpp"
#include "../vulkan/commands/draw.hpp"
#include <concepts>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::typed {
    template<typename...>
    class TypedBatch;
    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>, std::uint32_t subpass = 0>
    class TypedGraphicsPipeline;
    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>, bool bound = Resources::EMPTY, bool pushed = Pushes::EMPTY>
    class TypedVertexBinding;

    // One command explicitly binds the pipeline, every input binding, and then draws.
    // Captured owners survive wrapper destruction and are retained through GPU completion.
    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>>
    class TypedDraw final : public command::Command, public command::PipelineHolder {
        template<typename...>
        friend class TypedBatch;
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> _buffers;
        command::Draw _draw;
        std::vector<command::BindGraphicResource> _resources;
        std::optional<typename Pushes::Values> _pushes;
        TypedDraw(VSL_NAMESPACE::GraphicsPipeline pipeline, LogicalDeviceAccessor device,
                  std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> buffers, command::Draw draw, std::vector<command::BindGraphicResource> resources, std::optional<typename Pushes::Values> pushes)
            : _pipeline(std::move(pipeline)), _device(device), _buffers(std::move(buffers)), _draw(std::move(draw)), _resources(std::move(resources)), _pushes(std::move(pushes)) {}

    public:
        using VertexInput = Input;
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        PipelineAccessor get_pipeline() override { return _pipeline; }
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };

    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>>
    class TypedIndexedDraw final : public command::Command, public command::PipelineHolder {
        template<typename...>
        friend class TypedBatch;
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> _buffers;
        command::BindIndexBufferAt _indices;
        command::DrawIndexed _draw;
        std::vector<command::BindGraphicResource> _resources;
        std::optional<typename Pushes::Values> _pushes;
        std::shared_ptr<const std::uint64_t> _index_revision;
        std::uint64_t _expected_index_revision;
        void validate_indices() const;
        TypedIndexedDraw(VSL_NAMESPACE::GraphicsPipeline pipeline, LogicalDeviceAccessor device,
                         std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> buffers, command::BindIndexBufferAt indices,
                         command::DrawIndexed draw, std::vector<command::BindGraphicResource> resources, std::optional<typename Pushes::Values> pushes,
                         std::shared_ptr<const std::uint64_t> index_revision)
            : _pipeline(std::move(pipeline)), _device(device), _buffers(std::move(buffers)), _indices(std::move(indices)),
              _draw(std::move(draw)), _resources(std::move(resources)), _pushes(std::move(pushes)),
              _index_revision(std::move(index_revision)), _expected_index_revision(*_index_revision) {}

    public:
        using VertexInput = Input;
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        PipelineAccessor get_pipeline() override { return _pipeline; }
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    class TypedVertexBinding {
        template<typename, typename, typename, std::uint32_t>
        friend class TypedGraphicsPipeline;
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> _buffers;
        std::array<size_t, Input::BINDING_COUNT> _counts;
        std::vector<command::BindGraphicResource> _resources;
        std::optional<typename Pushes::Values> _pushes;
        template<typename Indices>
        TypedIndexedDraw<Input, Resources, Pushes> make_indexed_draw(const Indices& indices, size_t index_count,
                                                                     size_t instance_count, size_t first_index, std::int64_t vertex_offset, size_t first_instance) const;
        template<bool other_bound, bool other_pushed>
        TypedVertexBinding(const TypedVertexBinding<Input, Resources, Pushes, other_bound, other_pushed>& source, std::vector<command::BindGraphicResource> resources, std::optional<typename Pushes::Values> pushes)
            : _pipeline(source._pipeline), _device(source._device), _buffers(source._buffers), _counts(source._counts), _resources(std::move(resources)), _pushes(std::move(pushes)) {}
        template<typename... Buffers, size_t... indices>
        TypedVertexBinding(VSL_NAMESPACE::GraphicsPipeline pipeline, LogicalDeviceAccessor device,
                           std::index_sequence<indices...>, const Buffers&... buffers)
            : _pipeline(std::move(pipeline)), _device(device),
              _buffers{command::BindVertexBufferAt(Input::LAYOUT.binding[indices].binding, buffers.backend(), buffers.byte_offset(), buffers.resource_use())...}, _counts{buffers.size()...} {
            if (((buffers.device().data_ != device.data_) || ...))
                throw std::invalid_argument("Typed pipeline and vertex buffers use different devices");
        }

    public:
        using VertexInput = Input;
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        template<typename... Sets>
        requires compatible_descriptor_sets<Resources, Sets...>
        TypedVertexBinding<Input, Resources, Pushes, true, pushed> bind_resources(const Sets&... sets) const;
        TypedDraw<Input, Resources, Pushes> draw(size_t vertex_count, size_t instance_count = 1, size_t first_vertex = 0, size_t first_instance = 0) const
        requires(bound && pushed);
        template<index_element Index, MemoryProperty property>
        TypedIndexedDraw<Input, Resources, Pushes> draw_indexed(const TypedIndexBuffer<Index, property>& indices, size_t index_count,
                                                                size_t instance_count = 1, size_t first_index = 0, std::int64_t vertex_offset = 0, size_t first_instance = 0) const
        requires(bound && pushed);
        template<index_element Index>
        TypedIndexedDraw<Input, Resources, Pushes> draw_indexed(const TypedIndexBufferView<Index>& indices, size_t index_count,
                                                                size_t instance_count = 1, size_t first_index = 0, std::int64_t vertex_offset = 0, size_t first_instance = 0) const
        requires(bound && pushed);
        TypedVertexBinding<Input, Resources, Pushes, bound, true> push_constants(const typename Pushes::Values& values) const;
    };

    template<typename Input, typename Resources, typename Pushes, std::uint32_t subpass>
    class TypedGraphicsPipeline {
        // UINT32_MAX is VK_SUBPASS_EXTERNAL, never a graphics pipeline subpass.
        static_assert(subpass != UINT32_MAX, "Typed subpass index must not be narrowed to VK_SUBPASS_EXTERNAL");
        LogicalDeviceAccessor _device;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        template<pipeline_layout_injecter... Stages>
        static VSL_NAMESPACE::GraphicsPipeline MakePipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages);

    public:
        using VertexInput = Input;
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
        static constexpr std::uint32_t SUBPASS = subpass;
        template<pipeline_layout_injecter... Stages>
        TypedGraphicsPipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages)
            : _device(device), _pipeline(MakePipeline(device, std::move(pass), std::forward<Stages>(stages)...)) {}

        const VSL_NAMESPACE::GraphicsPipeline& backend() const { return _pipeline; }

        template<typename... Buffers>
        requires compatible_vertex_buffers<Input, Buffers...>
        TypedVertexBinding<Input, Resources, Pushes> bind_vertices(const Buffers&... buffers) const {
            return TypedVertexBinding<Input, Resources, Pushes>(_pipeline, _device, std::index_sequence_for<Buffers...>{}, buffers...);
        }
    };

    template<typename Input, typename Resources, typename Pushes, std::uint32_t subpass>
    template<pipeline_layout_injecter... Stages>
    VSL_NAMESPACE::GraphicsPipeline TypedGraphicsPipeline<Input, Resources, Pushes, subpass>::MakePipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages) {
        // Input is deliberately last: earlier stages cannot replace the typed vertex layout.
        PipelineLayout layout(device, std::forward<Stages>(stages)..., Input{}, Resources{}, Pushes{});
        detail::validate_pipeline_vertex_shader(layout, Input::LAYOUT.attribute);
        detail::validate_resource_schema(layout, Resources::REQUIREMENTS);
        detail::validate_push_schema(layout, Pushes::REQUIREMENTS);
        return VSL_NAMESPACE::GraphicsPipeline(std::move(layout).freeze(), pass, subpass);
    }

    template<typename Input, typename Resources, typename Pushes>
    void TypedDraw<Input, Resources, Pushes>::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (_device.data_ != manager.get_device().data_)
            throw std::invalid_argument("Typed pipeline and command manager use different devices");
        _pipeline.invoke(pool, buffer, manager);
        for (auto& binding : _buffers)
            binding.invoke(pool, buffer, manager);
        for (auto& resource : _resources)
            resource.invoke(pool, buffer, manager);
        if constexpr (!Pushes::EMPTY)
            detail::invoke_push_values(_pipeline, buffer, Pushes::REQUIREMENTS, _pushes->bytes());
        _draw.invoke(pool, buffer, manager);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    TypedDraw<Input, Resources, Pushes> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::draw(size_t vertex_count, size_t instance_count, size_t first_vertex, size_t first_instance) const
    requires(bound && pushed)
    {
        command::Draw command(vertex_count, instance_count, first_vertex, first_instance);
        for (size_t i = 0; i < Input::BINDING_COUNT; ++i) {
            const bool instanced = Input::LAYOUT.binding[i].timing == VertexTiming::NEXT_INSTANCE;
            const auto first = instanced ? first_instance : first_vertex;
            const auto count = instanced ? instance_count : vertex_count;
            if (first > _counts[i] || count > _counts[i] - first)
                throw std::out_of_range("Typed draw exceeds its vertex/instance buffer");
        }
        return TypedDraw<Input, Resources, Pushes>(_pipeline, _device, _buffers, std::move(command), _resources, _pushes);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    template<typename... Sets>
    requires compatible_descriptor_sets<Resources, Sets...>
    TypedVertexBinding<Input, Resources, Pushes, true, pushed> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::bind_resources(const Sets&... sets) const {
        if (((sets.device().data_ != _device.data_) || ...))
            throw std::invalid_argument("Typed descriptor/pipeline device mismatch");
        (sets.require_complete(), ...);
        std::vector<command::BindGraphicResource> commands;
        commands.reserve(sizeof...(Sets));
        size_t set_index = 0;
        (commands.emplace_back(sets.backend(), graphic_resource::BindingDestination::GRAPHICS, _pipeline,
                               Resources::SET_NUMBERS[set_index++], sets.resource_use()),
         ...);
        return TypedVertexBinding<Input, Resources, Pushes, true, pushed>(*this, std::move(commands), _pushes);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    TypedVertexBinding<Input, Resources, Pushes, bound, true> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::push_constants(const typename Pushes::Values& values) const {
        return TypedVertexBinding<Input, Resources, Pushes, bound, true>(*this, _resources, std::optional<typename Pushes::Values>(values));
    }

    template<typename Input, typename Resources, typename Pushes>
    void TypedIndexedDraw<Input, Resources, Pushes>::validate_indices() const {
        if (*_index_revision != _expected_index_revision)
            throw std::logic_error("Typed index data changed after draw command creation");
    }

    template<typename Input, typename Resources, typename Pushes>
    void TypedIndexedDraw<Input, Resources, Pushes>::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (_device.data_ != manager.get_device().data_)
            throw std::invalid_argument("Typed pipeline and command manager use different devices");
        validate_indices();
        _pipeline.invoke(pool, buffer, manager);
        for (auto& binding : _buffers)
            binding.invoke(pool, buffer, manager);
        _indices.invoke(pool, buffer, manager);
        for (auto& resource : _resources)
            resource.invoke(pool, buffer, manager);
        if constexpr (!Pushes::EMPTY)
            detail::invoke_push_values(_pipeline, buffer, Pushes::REQUIREMENTS, _pushes->bytes());
        _draw.invoke(pool, buffer, manager);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    template<index_element Index, MemoryProperty property>
    TypedIndexedDraw<Input, Resources, Pushes> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::draw_indexed(
        const TypedIndexBuffer<Index, property>& indices, size_t index_count, size_t instance_count, size_t first_index, std::int64_t vertex_offset, size_t first_instance) const
    requires(bound && pushed)
    {
        return make_indexed_draw(indices, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    template<index_element Index>
    TypedIndexedDraw<Input, Resources, Pushes> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::draw_indexed(
        const TypedIndexBufferView<Index>& indices, size_t index_count, size_t instance_count, size_t first_index, std::int64_t vertex_offset, size_t first_instance) const
    requires(bound && pushed)
    {
        return make_indexed_draw(indices, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    template<typename Indices>
    TypedIndexedDraw<Input, Resources, Pushes> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::make_indexed_draw(
        const Indices& indices, size_t index_count, size_t instance_count, size_t first_index, std::int64_t vertex_offset, size_t first_instance) const {
        command::DrawIndexed command(index_count, instance_count, first_index, vertex_offset, first_instance);
        if (indices.device().data_ != _device.data_)
            throw std::invalid_argument("Typed index buffer and pipeline use different devices");
        const auto [low, high] = indices.bounds(first_index, index_count);
        for (size_t i = 0; i < Input::BINDING_COUNT; ++i) {
            if (Input::LAYOUT.binding[i].timing == VertexTiming::NEXT_INSTANCE) {
                if (first_instance > _counts[i] || instance_count > _counts[i] - first_instance)
                    throw std::out_of_range("Typed indexed draw exceeds its instance buffer");
            } else if (index_count && instance_count && (std::int64_t(low) + vertex_offset < 0 || std::uint64_t(std::int64_t(high) + vertex_offset) >= _counts[i]))
                throw std::out_of_range("Typed indexed draw exceeds its vertex buffer");
        }
        return TypedIndexedDraw<Input, Resources, Pushes>(_pipeline, _device, _buffers,
                                                          command::BindIndexBufferAt(indices.backend(), Indices::INDEX_TYPE, indices.byte_offset(), indices.resource_use()), std::move(command), _resources, _pushes,
                                                          std::shared_ptr<const std::uint64_t>(indices._values, &indices._values->revision));
    }

} // namespace VSL_NAMESPACE::typed
