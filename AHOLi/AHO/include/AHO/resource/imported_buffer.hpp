#pragma once
#include "buffer.hpp"

namespace AHO_NAMESPACE::resource {
    // 物理領域を所有するviewを取り込み、論理サイズだけを容量内で変更する。
    class ImportedBuffer {
        std::shared_ptr<detail::BufferState> _state;
        friend class ArenaBuffer;

    protected:
        explicit ImportedBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device);

    public:
        explicit ImportedBuffer(VSL_NAMESPACE::BufferView storage);
        BufferBinding binding() const;
        size_t size() const { return _state ? _state->size : 0; }
        size_t capacity() const { return _state && _state->current ? _state->current->size() : 0; }
        bool busy() const { return _state && _state->busy(); }
        bool wait(std::uint64_t timeout = UINT64_MAX) const;
        void resize(size_t size);
        void assign(std::span<const std::byte> bytes);
        void write(std::span<const std::byte> bytes, size_t offset = 0);
        void read_into(std::span<std::byte> destination, size_t offset = 0) const;
        AHO_NAMESPACE::command::BufferUpload upload(const Buffer<BufferType::GENERAL>& source, size_t destination_offset = 0) const;
        template<gpu_buffer Source>
        AHO_NAMESPACE::command::BufferUpload upload(const Source& source, size_t destination_offset = 0, size_t source_offset = 0, std::optional<size_t> bytes = std::nullopt) const;
    };

    ImportedBuffer import_buffer(VSL_NAMESPACE::BufferView storage);
} // namespace AHO_NAMESPACE::resource
