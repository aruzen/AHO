#pragma once
#include "buffer.hpp"

namespace AHO_NAMESPACE::resource {
    namespace detail {
        // 公開型同士の変換を作らず、解決済みviewへの操作だけを共用する。
        class ViewBufferBase {
            std::shared_ptr<BufferState> _state;

        protected:
            explicit ViewBufferBase(std::shared_ptr<BufferState> state);
            explicit ViewBufferBase(VSL_NAMESPACE::BufferView storage);
            BufferState& state() const;
            std::shared_ptr<BufferState> state_owner() const {
                (void)state();
                return _state;
            }

        public:
            BufferBinding binding() const;
            size_t size() const { return _state ? _state->size : 0; }
            size_t capacity() const { return _state && _state->current ? _state->current->size() : 0; }
            bool busy() const {
                const auto state = _state;
                return state && state->busy();
            }
            bool wait(std::uint64_t timeout = UINT64_MAX) const;
            void resize(size_t size);
            void assign(std::span<const std::byte> bytes);
            void write(std::span<const std::byte> bytes, size_t offset = 0);
            void read_into(std::span<std::byte> destination, size_t offset = 0) const;
            AHO_NAMESPACE::command::BufferUpload upload(const Buffer<BufferType::GENERAL>& source, size_t destination_offset = 0) const;
            template<gpu_buffer Source>
            AHO_NAMESPACE::command::BufferUpload upload(const Source& source, size_t destination_offset = 0, size_t source_offset = 0, std::optional<size_t> bytes = std::nullopt) const;
        };
    } // namespace detail

    // 物理領域を所有するviewを取り込み、論理サイズだけを容量内で変更する。
    class ImportedBuffer : private detail::ViewBufferBase {
        using Base = detail::ViewBufferBase;

    public:
        explicit ImportedBuffer(VSL_NAMESPACE::BufferView storage);
        using Base::binding;
        using Base::size;
        using Base::capacity;
        using Base::busy;
        using Base::wait;
        using Base::resize;
        using Base::assign;
        using Base::write;
        using Base::read_into;
        using Base::upload;
    };

    ImportedBuffer import_buffer(VSL_NAMESPACE::BufferView storage);
} // namespace AHO_NAMESPACE::resource
