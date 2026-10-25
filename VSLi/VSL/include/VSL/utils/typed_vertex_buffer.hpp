#pragma once
#include "typed_vertex_layout.hpp"
#include "typed_buffer_view.hpp"
#include "../vulkan/buffer.hpp"
#include <concepts>
#include <span>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::typed {
    // Vertex storage and views are independent of a pipeline's binding numbers.
    template<typename Vertex_, MemoryProperty memory_property>
    class TypedVertexBuffer;
    template<typename Vertex_>
    class TypedVertexBufferView {
        template<typename, MemoryProperty>
        friend class TypedVertexBuffer;
        LogicalDeviceAccessor _device;
        std::shared_ptr<impl::BufferImpl> _buffer;
        size_t _offset, _count;
        TypedVertexBufferView(LogicalDeviceAccessor device, std::shared_ptr<impl::BufferImpl> buffer, size_t offset, size_t count)
            : _device(device), _buffer(std::move(buffer)), _offset(offset), _count(count) {}

    public:
        using Vertex = Vertex_;
        size_t size() const { return _count; }
        size_t byte_offset() const { return _offset; }
        LogicalDeviceAccessor device() const { return _device; }
        const std::shared_ptr<impl::BufferImpl>& backend() const { return _buffer; }
        TypedVertexBufferView view(size_t first, size_t count) const {
            return TypedVertexBufferView(_device, _buffer, detail::checked_view_offset(_offset, first, count, _count, sizeof(Vertex)), count);
        }
    };

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
        size_t byte_offset() const { return 0; }
        TypedVertexBufferView<Vertex> view(size_t first, size_t count) const {
            return TypedVertexBufferView<Vertex>(_device, _buffer.data_, detail::checked_view_offset(0, first, count, _count, sizeof(Vertex)), count);
        }
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
