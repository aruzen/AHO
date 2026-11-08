#pragma once
#include "imported_buffer.hpp"
#include <functional>

namespace AHO_NAMESPACE::resource {
    // factoryの返却値は領域・device・allocator等の依存を保持するowning viewであること。
    using ArenaBufferFactory = std::function<VSL_NAMESPACE::BufferView(VSL_NAMESPACE::LogicalDeviceAccessor, size_t)>;

    class ArenaBuffer : private detail::ViewBufferBase {
        using Base = detail::ViewBufferBase;
        using RetiredStorage = std::pair<std::optional<VSL_NAMESPACE::BufferView>, std::shared_ptr<VSL_NAMESPACE::BufferAccessor>>;
        static VSL_NAMESPACE::BufferView Prepare(const std::shared_ptr<detail::BufferState>& state, size_t bytes, bool preserve);
        static RetiredStorage Publish(detail::BufferState& state, VSL_NAMESPACE::BufferView storage);

    public:
        explicit ArenaBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size = 0, ArenaBufferFactory factory = {});
        using Base::binding;
        using Base::size;
        using Base::capacity;
        using Base::busy;
        using Base::wait;
        using Base::write;
        using Base::read_into;
        using Base::upload;
        void reserve(size_t capacity);
        void resize(size_t size);
        void assign(std::span<const std::byte> bytes);
    };

    ArenaBuffer make_arena_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size = 0, ArenaBufferFactory factory = {});
} // namespace AHO_NAMESPACE::resource
