#pragma once
#include "../utils/typed_resources.hpp"
#include "buffer_view.hpp"

namespace VSL_NAMESPACE::typed {
    template<typename Payload_, graphic_resource::Type kind>
    class PlacedResourceBufferView {
        BufferView _view;

    public:
        using Payload = typename PayloadLayout<Payload_>::Payload;
        static constexpr auto KIND = kind;
        static_assert(std::is_trivially_copyable_v<Payload> && std::is_standard_layout_v<Payload>);
        static_assert(kind == graphic_resource::Type::UNIFORM_BUFFER || kind == graphic_resource::Type::STORAGE_BUFFER);
        explicit PlacedResourceBufferView(BufferView view);
        const BufferView& backend() const { return _view; }
        size_t size() const { return _view.size() / sizeof(Payload); }
        void copy(std::span<const Payload> values);
        void read_into(std::span<Payload> values) const;
        PlacedResourceBufferView view(size_t first, size_t count) const;
        detail::DescriptorValue descriptor() const { return detail::DescriptorValue::FromBuffer(_view); }
    };

    template<typename Payload_, graphic_resource::Type kind>
    PlacedResourceBufferView<Payload_, kind>::PlacedResourceBufferView(BufferView view) : _view(std::move(view)) {
        constexpr auto usage = kind == graphic_resource::Type::UNIFORM_BUFFER ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER;
        if (!contain(_view.ref().usage(), usage) || _view.size() % sizeof(Payload) || _view.byte_offset() % alignof(Payload))
            throw std::invalid_argument("Placed resource view payload/usage mismatch");
    }
    template<typename Payload_, graphic_resource::Type kind>
    void PlacedResourceBufferView<Payload_, kind>::copy(std::span<const Payload> values) {
        if (values.size() != size())
            throw std::invalid_argument("Typed descriptor buffer update must preserve record count");
        _view.write(std::as_bytes(values));
    }
    template<typename Payload_, graphic_resource::Type kind>
    void PlacedResourceBufferView<Payload_, kind>::read_into(std::span<Payload> values) const {
        if (values.size() != size())
            throw std::invalid_argument("Typed descriptor buffer read must preserve record count");
        _view.read_into(std::as_writable_bytes(values));
    }
    template<typename Payload_, graphic_resource::Type kind>
    PlacedResourceBufferView<Payload_, kind> PlacedResourceBufferView<Payload_, kind>::view(size_t first, size_t count) const {
        const auto offset = detail::checked_view_offset(0, first, count, size(), sizeof(Payload));
        return PlacedResourceBufferView(_view.view(offset, count * sizeof(Payload)));
    }
} // namespace VSL_NAMESPACE::typed
