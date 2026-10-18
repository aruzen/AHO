#pragma once
#include "buffer_and_image_accessor.hpp"
#include <cstddef>
#include <span>

namespace VSL_NAMESPACE {
    // Host-visible GPU buffer with one persistent mapping. Copies share ownership.
    struct MappedBuffer {
        std::shared_ptr<_impl::MappedBuffer_impl> _data;
        explicit MappedBuffer(LogicalDeviceAccessor device, std::size_t bytes,
                              MemoryType usage = MemoryType::TransferDestination);
        std::size_t size() const;
        // Caller must first wait for the GPU writer. Invalidates non-coherent memory.
        std::span<const std::byte> read(std::size_t bytes, std::size_t offset = 0) const;
    };
}
