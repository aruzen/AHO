#pragma once
#include "typed_vertex_layout.hpp"
#include "typed_buffer_view.hpp"
#include "../vulkan/buffer.hpp"
#include "../vulkan/resource_use.hpp"
#include <concepts>
#include <span>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::typed {
    // 頂点storageとviewはPipelineのbinding番号から独立させる。
    template<typename Vertex_, MemoryProperty memory_property>
    class TypedVertexBuffer;
    template<typename Vertex_>
    class TypedVertexBufferView {
        template<typename, MemoryProperty>
        friend class TypedVertexBuffer;
        BufferView _buffer;
        size_t _count;
        ResourceUse _use;
        TypedVertexBufferView(BufferView buffer, size_t count, ResourceUse use)
            : _buffer(std::move(buffer)), _count(count), _use(std::move(use)) {}

    public:
        using Vertex = Vertex_;
        static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>,
                      "Typed vertex records must be standard-layout and trivially_copyable");
        static TypedVertexBufferView FromBuffer(const BufferAccessor& source, size_t count, size_t byte_offset = 0, ResourceUse use = {});
        static TypedVertexBufferView FromView(const BufferView& source, size_t count, size_t byte_offset = 0, ResourceUse use = {});
        const ResourceUse& resource_use() const { return _use; }
        size_t size() const { return _count; }
        size_t byte_offset() const { return _buffer.byte_offset(); }
        LogicalDeviceAccessor device() const { return _buffer.ref().device(); }
        const BufferView& backend() const& { return _buffer; }
        const BufferView& backend() const&& = delete;
        BufferView storage_view() const { return _buffer; }
        TypedVertexBufferView view(size_t first, size_t count) const {
            return TypedVertexBufferView(_buffer.view(detail::checked_view_offset(0, first, count, _count, sizeof(Vertex)), count * sizeof(Vertex)), count, _use);
        }
    };

    template<typename Vertex_, MemoryProperty memory_property = MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT>
    class TypedVertexBuffer {
    public:
        using Vertex = Vertex_;

    private:
        static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>);
        static constexpr auto USAGE = MemoryType::VERTEX_BUFFER | MemoryType::TRANSFER_DESTINATION;
        BufferView _buffer;
        size_t _count;

    public:
        template<buffer_storage_policy Policy = DefaultStorage>
        TypedVertexBuffer(LogicalDeviceAccessor device, std::span<const Vertex> vertices, Policy policy = {})
        requires(contain<memory_property, MemoryProperty::HOST_VISIBLE>());

        template<buffer_storage_policy Policy = DefaultStorage>
        TypedVertexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Vertex> vertices, Policy policy = {})
        requires(!contain<memory_property, MemoryProperty::HOST_VISIBLE>());

        size_t size() const noexcept { return _count; }
        size_t byte_offset() const { return _buffer.byte_offset(); }
        TypedVertexBufferView<Vertex> view(size_t first, size_t count) const {
            return TypedVertexBufferView<Vertex>(_buffer.view(detail::checked_view_offset(0, first, count, _count, sizeof(Vertex)), count * sizeof(Vertex)), count, resource_use());
        }
        LogicalDeviceAccessor device() const { return _buffer.ref().device(); }
        ResourceUse resource_use() const { return _buffer.resource_use(); }
        const BufferView& backend() const& { return _buffer; }
        const BufferView& backend() const&& = delete;
        BufferView storage_view() const { return _buffer; }
        void copy(std::span<const Vertex> vertices)
        requires(contain<memory_property, MemoryProperty::HOST_VISIBLE>());
    };

    template<typename Vertex, MemoryProperty memory_property>
    template<buffer_storage_policy Policy>
    TypedVertexBuffer<Vertex, memory_property>::TypedVertexBuffer(LogicalDeviceAccessor device, std::span<const Vertex> vertices, Policy policy)
    requires(contain<memory_property, MemoryProperty::HOST_VISIBLE>())
        : _buffer(detail::make_buffer_storage(device, {helper::checked_buffer_array_size<Vertex>(vertices.size()), USAGE, memory_property}, policy)), _count(vertices.size()) {
        copy(vertices);
    }
    template<typename Vertex, MemoryProperty memory_property>
    template<buffer_storage_policy Policy>
    TypedVertexBuffer<Vertex, memory_property>::TypedVertexBuffer(LogicalDeviceAccessor device, CommandManager manager, std::span<const Vertex> vertices, Policy policy)
    requires(!contain<memory_property, MemoryProperty::HOST_VISIBLE>())
        : _buffer(detail::make_buffer_storage(device, std::move(manager), {helper::checked_buffer_array_size<Vertex>(vertices.size()), USAGE, memory_property}, std::as_bytes(vertices), policy)), _count(vertices.size()) {}

    template<typename Vertex, MemoryProperty memory_property>
    void TypedVertexBuffer<Vertex, memory_property>::copy(std::span<const Vertex> vertices)
    requires(contain<memory_property, MemoryProperty::HOST_VISIBLE>())
    {
        const auto buffer = _buffer;
        const auto count = _count;
        buffer.ref().use().require_idle();
        if (vertices.size() != count)
            throw std::invalid_argument("Typed vertex update must preserve the record count");
        buffer.write(std::as_bytes(vertices));
    }

    template<typename Vertex>
    TypedVertexBufferView<Vertex> TypedVertexBufferView<Vertex>::FromBuffer(const BufferAccessor& source, size_t count, size_t byte_offset, ResourceUse use) {
        if (!source.data_)
            throw std::invalid_argument("Typed buffer view requires a buffer");
        return FromView(BufferView::FromBuffer(source), count, byte_offset, std::move(use));
    }
    template<typename Vertex>
    TypedVertexBufferView<Vertex> TypedVertexBufferView<Vertex>::FromView(const BufferView& source, size_t count, size_t byte_offset, ResourceUse use) {
        detail::validate_buffer_view(source, MemoryType::VERTEX_BUFFER, byte_offset, count, sizeof(Vertex), alignof(Vertex));
        return TypedVertexBufferView(source.view(byte_offset, count * sizeof(Vertex)), count, source.resource_use(std::move(use)));
    }

    namespace detail {
        template<typename T>
        struct IsVertexBuffer : std::false_type {};
        template<typename Vertex, MemoryProperty property>
        struct IsVertexBuffer<TypedVertexBuffer<Vertex, property>> : std::true_type {};

        template<typename Vertex>
        struct IsVertexBuffer<TypedVertexBufferView<Vertex>> : std::true_type {};

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

} // namespace VSL_NAMESPACE::typed
