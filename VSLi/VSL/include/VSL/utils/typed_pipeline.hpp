#pragma once
#include "typed_shader.hpp"
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

    template<typename Input>
    class TypedGraphicsPipeline;
    template<typename Input>
    class TypedVertexBinding;

    // One command explicitly binds the pipeline, every input binding, and then draws.
    // Captured owners survive wrapper destruction and are retained through GPU completion.
    template<typename Input>
    class TypedDraw final : public command::Command, public command::PipelineHolder {
        friend class TypedVertexBinding<Input>;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> _buffers;
        command::Draw _draw;
        TypedDraw(VSL_NAMESPACE::GraphicsPipeline pipeline, LogicalDeviceAccessor device,
                  std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> buffers, command::Draw draw)
            : _pipeline(std::move(pipeline)), _device(device), _buffers(std::move(buffers)), _draw(std::move(draw)) {}

    public:
        using VertexInput = Input;
        PipelineAccessor get_pipeline() override { return _pipeline; }
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };

    template<typename Input>
    class TypedVertexBinding {
        friend class TypedGraphicsPipeline<Input>;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        LogicalDeviceAccessor _device;
        std::array<command::BindVertexBufferAt, Input::BINDING_COUNT> _buffers;
        std::array<size_t, Input::BINDING_COUNT> _counts;
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
        TypedDraw<Input> draw(size_t vertex_count, size_t instance_count = 1, size_t first_vertex = 0, size_t first_instance = 0) const;
    };

    template<typename Input>
    class TypedGraphicsPipeline {
        LogicalDeviceAccessor _device;
        VSL_NAMESPACE::GraphicsPipeline _pipeline;
        template<pipeline_layout_injecter... Stages>
        static VSL_NAMESPACE::GraphicsPipeline MakePipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages) {
            // Input is deliberately last: earlier stages cannot replace the typed vertex layout.
            PipelineLayout layout(device, std::forward<Stages>(stages)..., Input{});
            detail::validate_pipeline_vertex_shader(layout, Input::LAYOUT.attribute);
            return VSL_NAMESPACE::GraphicsPipeline(std::move(layout).freeze(), pass);
        }

    public:
        using VertexInput = Input;
        template<pipeline_layout_injecter... Stages>
        TypedGraphicsPipeline(LogicalDeviceAccessor device, RenderPassAccessor pass, Stages&&... stages)
            : _device(device), _pipeline(MakePipeline(device, std::move(pass), std::forward<Stages>(stages)...)) {}

        const VSL_NAMESPACE::GraphicsPipeline& backend() const { return _pipeline; }

        template<typename... Buffers>
        requires compatible_vertex_buffers<Input, Buffers...>
        TypedVertexBinding<Input> bind_vertices(const Buffers&... buffers) const {
            return TypedVertexBinding<Input>(_pipeline, _device, std::index_sequence_for<Buffers...>{}, buffers...);
        }
    };

    template<typename Input>
    void TypedDraw<Input>::invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) {
        if (_device.data_ != manager.get_device().data_)
            throw std::invalid_argument("Typed pipeline and command manager use different devices");
        _pipeline.invoke(pool, buffer, manager);
        for (auto& binding : _buffers)
            binding.invoke(pool, buffer, manager);
        _draw.invoke(pool, buffer, manager);
    }

    template<typename Input>
    TypedDraw<Input> TypedVertexBinding<Input>::draw(size_t vertex_count, size_t instance_count, size_t first_vertex, size_t first_instance) const {
        command::Draw command(vertex_count, instance_count, first_vertex, first_instance);
        for (size_t i = 0; i < Input::BINDING_COUNT; ++i) {
            const bool instanced = Input::LAYOUT.binding[i].timing == VertexTiming::NEXT_INSTANCE;
            const auto first = instanced ? first_instance : first_vertex;
            const auto count = instanced ? instance_count : vertex_count;
            if (first > _counts[i] || count > _counts[i] - first)
                throw std::out_of_range("Typed draw exceeds its vertex/instance buffer");
        }
        return TypedDraw<Input>(_pipeline, _device, _buffers, std::move(command));
    }

} // namespace VSL_NAMESPACE::typed
