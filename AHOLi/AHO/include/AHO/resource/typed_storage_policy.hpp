#pragma once
#include "typed_resource_buffer.hpp"
#include <VSL/utils/typed_resource_storage.hpp>

namespace AHO_NAMESPACE::resource {
    struct HostBufferFactory {
        template<typename Binding, typename Allocation>
        Buffer<BufferType::GPU_FRIENDLY> make_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t bytes, size_t element) const;
    };

    template<typename Factory = HostBufferFactory>
    struct TypedStoragePolicy {
        Factory factory;
        template<typename Binding, typename Allocation>
        using Storage = std::remove_cvref_t<decltype(std::declval<Factory&>().template make_buffer<Binding, Allocation>(std::declval<VSL_NAMESPACE::LogicalDeviceAccessor>(), size_t{}, size_t{}))>;
        template<typename Binding, typename Allocation>
        using Buffer = TypedResourceBuffer<typename Binding::Payload, Binding::POINT.binding_type, Storage<Binding, Allocation>>;
        template<typename Binding, typename Allocation>
        Buffer<Binding, Allocation> make_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t count, size_t element);
    };

    template<typename Binding, typename Allocation>
    auto HostBufferFactory::make_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t bytes, size_t) const -> Buffer<BufferType::GPU_FRIENDLY> {
        return Buffer<BufferType::GPU_FRIENDLY>(device, bytes);
    }
    template<typename Factory>
    template<typename Binding, typename Allocation>
    auto TypedStoragePolicy<Factory>::make_buffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t count, size_t element) -> Buffer<Binding, Allocation> {
        const auto bytes = VSL_NAMESPACE::helper::checked_buffer_array_size<typename Binding::Payload>(count);
        return Buffer<Binding, Allocation>(factory.template make_buffer<Binding, Allocation>(device, bytes, element), std::in_place, count);
    }
} // namespace AHO_NAMESPACE::resource
