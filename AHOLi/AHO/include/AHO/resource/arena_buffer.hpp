#pragma once
#include "imported_buffer.hpp"
#include <functional>

namespace AHO_NAMESPACE::resource {
    namespace detail {
        struct ArenaBufferProvider;
    }
    // factoryの返却値は領域・device・allocator等の依存を保持するowning viewであること。
    using ArenaBufferFactory = std::function<VSL_NAMESPACE::BufferView(VSL_NAMESPACE::LogicalDeviceAccessor, size_t)>;

    class ArenaBuffer : public ImportedBuffer {
        std::shared_ptr<detail::ArenaBufferProvider> _provider;
        VSL_NAMESPACE::BufferView allocate(size_t bytes) const;
        void publish(VSL_NAMESPACE::BufferView storage);
        void replace(size_t bytes, bool preserve);

    public:
        explicit ArenaBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size = 0, ArenaBufferFactory factory = {});
        void reserve(size_t capacity);
        void resize(size_t size);
        void assign(std::span<const std::byte> bytes);
    };

    ArenaBuffer make_arena_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size = 0, ArenaBufferFactory factory = {});
} // namespace AHO_NAMESPACE::resource
