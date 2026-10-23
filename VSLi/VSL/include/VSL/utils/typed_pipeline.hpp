#pragma once
#include "typed_shader.hpp"
#include "typed_resources.hpp"
#include "typed_push_constants.hpp"
#include "../vulkan/commands/bind_graphic_resource.hpp"
#include "../vulkan/buffer.hpp"
#include "../vulkan/pipeline.hpp"
#include "../vulkan/commands/bind_vertex_buffers.hpp"
#include "../vulkan/commands/draw.hpp"
#include <concepts>
#include <span>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::typed {
    template<typename Vertex_, MemoryProperty memory_property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    class TypedVertexBuffer {
    public:
        using Vertex = Vertex_;

    private:
        static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>);
        using Storage = VSL_NAMESPACE::Buffer<MemoryType::VERTEX_BUFFER | MemoryType::TRANSFER_DESTINATION, memory_property>;
        LogicalDeviceAccessor _device;
        Storage _buffer;
        size_t _count;

    public:
        TypedVertexBuffer(LogicalDeviceAccessor device, std::span<const Vertex> vertices)
        requires(contain<memory_property, MemoryProperty::HOST_VISIBLE>())
            : _device(device), _buffer(device, VSL_NAMESPACE::helper::checked_buffer_array_size<Vertex>(vertices.size())), _count(vertices.size()) {
            copy(vertices);
        }

        TypedVertexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Vertex> vertices)
        requires(!contain<memory_property, MemoryProperty::HOST_VISIBLE>())
            : _device(device), _buffer(device, manager, vertices.data(), vertices.size()), _count(vertices.size()) {}

        size_t size() const noexcept { return _count; }
        LogicalDeviceAccessor device() const { return _device; }
        // Explicit escape to the existing dynamic API. Typed binding does not accept a raw BufferAccessor.
        Storage& backend() { return _buffer; }
        const Storage& backend() const { return _buffer; }
        void copy(std::span<const Vertex> vertices)
        requires(contain<memory_property, MemoryProperty::HOST_VISIBLE>())
        {
            if (vertices.size() != _count)
                throw std::invalid_argument("Typed vertex update must preserve the record count");
            if (!_buffer.copy(vertices.data(), vertices.size()))
                throw std::runtime_error("Typed vertex upload failed");
        }
    };

    namespace detail {
        template<typename T>
        struct IsVertexBuffer : std::false_type {};
        template<typename Vertex, MemoryProperty property>
        struct IsVertexBuffer<TypedVertexBuffer<Vertex, property>> : std::true_type {};

        template<typename Input, typename... Buffers, size_t... indices>
        consteval bool vertex_buffers_match(std::index_sequence<indices...>) {
            if constexpr (sizeof...(Buffers) != Input::BINDING_COUNT || !(IsVertexBuffer<std::remove_cvref_t<Buffers>>::value && ...))
                return false;
            else if constexpr (!(requires { typename Input::template Shape<indices>::Vertex; } && ...))
                return false;
            else
                return (std::same_as<typename std::remove_cvref_t<Buffers>::Vertex, typename Input::template Shape<indices>::Vertex> && ...);
        }
    } // namespace detail

    template<typename Input, typename... Buffers>
    concept compatible_vertex_buffers = detail::vertex_buffers_match<Input, Buffers...>(std::index_sequence_for<Buffers...>{});

    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>>
    class TypedGraphicsPipeline;
    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>, bool bound = Resources::EMPTY, bool pushed = Pushes::EMPTY>
    class TypedVertexBinding;

    // One command explicitly binds the pipeline, every input binding, and then draws.
    // Captured owners survive wrapper destruction and are retained through GPU completion.
    template<typename Input, typename Resources = ResourceSchema<>, typename Pushes = PushSchema<>>
    class TypedDraw final : public command::Command, public command::PipelineHolder {
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

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    class TypedVertexBinding {
        friend class TypedGraphicsPipeline<Input, Resources, Pushes>;
        template<typename, typename, typename, bool, bool>
        friend class TypedVertexBinding;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> _buffers;
        std::array<size_t, Input::BINDING_COUNT> _counts;
        std::vector<command::BindGraphicResource> _resources;
        std::optional<typename Pushes::Values> _pushes;
        template<bool other_bound, bool other_pushed>
        TypedVertexBinding(const TypedVertexBinding<Input, Resources, Pushes, other_bound, other_pushed>& source, std::vector<command::BindGraphicResource> resources, std::optional<typename Pushes::Values> pushes)
            : _pipeline(source._pipeline), _device(source._device), _buffers(source._buffers), _counts(source._counts), _resources(std::move(resources)), _pushes(std::move(pushes)) {}
        template<typename... Buffers, size_t... indices>
        TypedVertexBinding(VSL_NAMESPACE::GraphicsPipeline pipeline, LogicalDeviceAccessor device,
                           std::index_sequence<indices...>, const Buffers&... buffers)
            : _pipeline(std::move(pipeline)), _device(device),
              _buffers{command::BindVertexBufferAt(Input::LAYOUT.binding[indices].binding, buffers.backend())...}, _counts{buffers.size()...} {
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
        TypedVertexBinding<Input, Resources, Pushes, bound, true> push_constants(const typename Pushes::Values& values) const;
    };

    template<typename Input, typename Resources, typename Pushes>
    class TypedGraphicsPipeline {
        LogicalDeviceAccessor _device;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        template<pipeline_layout_injecter... Stages>
        static VSL_NAMESPACE::GraphicsPipeline MakePipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages);

    public:
        using VertexInput = Input;
        using ResourceLayout = Resources;
        using PushLayout = Pushes;
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

    template<typename Input, typename Resources, typename Pushes>
    template<pipeline_layout_injecter... Stages>
    VSL_NAMESPACE::GraphicsPipeline TypedGraphicsPipeline<Input, Resources, Pushes>::MakePipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages) {
        // Input is deliberately last: earlier stages cannot replace the typed vertex layout.
        PipelineLayout layout(device, std::forward<Stages>(stages)..., Input{}, Resources{}, Pushes{});
        detail::validate_pipeline_vertex_shader(layout, Input::LAYOUT.attribute);
        detail::validate_resource_schema(layout, Resources::REQUIREMENTS);
        detail::validate_push_schema(layout, Pushes::REQUIREMENTS);
        return VSL_NAMESPACE::GraphicsPipeline(std::move(layout).freeze(), pass);
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
                               Resources::SET_NUMBERS[set_index++]),
         ...);
        return TypedVertexBinding<Input, Resources, Pushes, true, pushed>(*this, std::move(commands), _pushes);
    }

    template<typename Input, typename Resources, typename Pushes, bool bound, bool pushed>
    TypedVertexBinding<Input, Resources, Pushes, bound, true> TypedVertexBinding<Input, Resources, Pushes, bound, pushed>::push_constants(const typename Pushes::Values& values) const {
        return TypedVertexBinding<Input, Resources, Pushes, bound, true>(*this, _resources, std::optional<typename Pushes::Values>(values));
    }

} // namespace VSL_NAMESPACE::typed
