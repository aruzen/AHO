#pragma once
#include "typed_buffer.hpp"

namespace AHO_NAMESPACE::resource {
    namespace detail {
        template<typename Storage>
        concept writable_gpu_buffer = gpu_buffer<Storage> && requires(Storage& storage, std::span<const std::byte> bytes) {
            { storage.write(bytes, size_t{}) } -> std::same_as<void>;
        };
        inline constexpr std::array<std::byte, 4096> ZEROED_BUFFER_BLOCK{};
        template<writable_gpu_buffer Storage>
        void zero_typed_resource_buffer(Storage& storage, size_t bytes);
    } // namespace detail

    template<typename Payload_, VSL_NAMESPACE::graphic_resource::Type kind, detail::writable_gpu_buffer Storage_>
    class TypedResourceBuffer {
        static_assert(kind == VSL_NAMESPACE::graphic_resource::Type::UNIFORM_BUFFER || kind == VSL_NAMESPACE::graphic_resource::Type::STORAGE_BUFFER);
        static constexpr auto USAGE = kind == VSL_NAMESPACE::graphic_resource::Type::UNIFORM_BUFFER ? VSL_NAMESPACE::MemoryType::UNIFORM_BUFFER : VSL_NAMESPACE::MemoryType::STORAGE_BUFFER;
        static_assert(detail::typed_buffer_usage<Storage_, USAGE>(), "Typed resource storage has incompatible buffer usage");
        using View = VSL_NAMESPACE::typed::TypedResourceBufferView<Payload_, kind>;
        Storage_ _storage;
        View _view;

    public:
        using Payload = typename View::Payload;
        using Storage = Storage_;
        static constexpr auto KIND = kind;
        TypedResourceBuffer(Storage storage, std::in_place_t, size_t count);
        size_t size() const { return _view.size(); }
        Storage& backend() { return _storage; }
        const Storage& backend() const { return _storage; }
        auto descriptor() const { return _view.descriptor(); }
        View view(size_t first, size_t count) const { return _view.view(first, count); }
        void copy(std::span<const Payload> values);
    };

    namespace detail {
        template<writable_gpu_buffer Storage>
        void zero_typed_resource_buffer(Storage& storage, size_t bytes) {
            for (size_t offset = 0; offset < bytes;) {
                const auto count = std::min(bytes - offset, ZEROED_BUFFER_BLOCK.size());
                storage.write(std::span(ZEROED_BUFFER_BLOCK).first(count), offset);
                offset += count;
            }
        }
    } // namespace detail

    template<typename Payload_, VSL_NAMESPACE::graphic_resource::Type kind, detail::writable_gpu_buffer Storage>
    TypedResourceBuffer<Payload_, kind, Storage>::TypedResourceBuffer(Storage storage, std::in_place_t, size_t count)
        : _storage(std::move(storage)), _view(detail::make_resource_view<Payload_, kind>(_storage, 0, count)) {
        detail::zero_typed_resource_buffer(_storage, _view.descriptor().bytes);
    }
    template<typename Payload_, VSL_NAMESPACE::graphic_resource::Type kind, detail::writable_gpu_buffer Storage>
    void TypedResourceBuffer<Payload_, kind, Storage>::copy(std::span<const Payload> values) {
        if (values.size() != size())
            throw std::invalid_argument("Typed AHO buffer update must preserve record count");
        if (_storage.binding().native().data_ != _view.descriptor().buffer)
            throw std::logic_error("Typed AHO buffer storage was replaced");
        _storage.write(std::as_bytes(values));
    }
} // namespace AHO_NAMESPACE::resource
