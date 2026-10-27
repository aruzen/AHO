#pragma once
#include "buffer.hpp"
#include <VSL/utils/typed_vertex_buffer.hpp>
#include <VSL/utils/typed_resources.hpp>

namespace AHO_NAMESPACE::resource {
    namespace detail {
        struct TypedBufferRange {
            size_t offset, count;
        };
        template<typename Source, VSL_NAMESPACE::MemoryType usage>
        consteval bool typed_buffer_usage();
        template<typename Payload>
        TypedBufferRange typed_buffer_range(size_t bytes, size_t first, std::optional<size_t> count);
        template<typename PayloadTag, VSL_NAMESPACE::graphic_resource::Type kind, gpu_buffer Source>
        auto make_resource_view(const Source& source, size_t first, std::optional<size_t> count);
    } // namespace detail

    template<typename PayloadTag, gpu_buffer Source>
    requires(detail::typed_buffer_usage<Source, VSL_NAMESPACE::MemoryType::UNIFORM_BUFFER>())
    auto uniform_view(const Source& source, size_t first = 0, std::optional<size_t> count = std::nullopt);
    template<typename PayloadTag, gpu_buffer Source>
    requires(detail::typed_buffer_usage<Source, VSL_NAMESPACE::MemoryType::STORAGE_BUFFER>())
    auto storage_view(const Source& source, size_t first = 0, std::optional<size_t> count = std::nullopt);
    template<typename Vertex, gpu_buffer Source>
    requires(detail::typed_buffer_usage<Source, VSL_NAMESPACE::MemoryType::VERTEX_BUFFER>())
    auto vertex_view(const Source& source, size_t first = 0, std::optional<size_t> count = std::nullopt);

    namespace detail {
        template<typename Source, VSL_NAMESPACE::MemoryType usage>
        consteval bool typed_buffer_usage() {
            if constexpr (requires { std::remove_cvref_t<decltype(std::declval<const Source&>().native())>::TYPE; }) {
                using Native = std::remove_cvref_t<decltype(std::declval<const Source&>().native())>;
                return VSL_NAMESPACE::contain(Native::TYPE, usage);
            } else
                return true;
        }
        template<typename Payload>
        TypedBufferRange typed_buffer_range(size_t bytes, size_t first, std::optional<size_t> count) {
            const auto total = bytes / sizeof(Payload);
            if (first > total)
                throw std::out_of_range("Typed AHO view starts outside initialized buffer data");
            if (!count && bytes % sizeof(Payload))
                throw std::invalid_argument("Typed AHO view requires whole payload records or an explicit count");
            const auto records = count.value_or(total - first);
            return {VSL_NAMESPACE::typed::detail::checked_view_offset(0, first, records, total, sizeof(Payload)), records};
        }
        template<typename PayloadTag, VSL_NAMESPACE::graphic_resource::Type kind, gpu_buffer Source>
        auto make_resource_view(const Source& source, size_t first, std::optional<size_t> count) {
            using Payload = typename VSL_NAMESPACE::typed::PayloadLayout<PayloadTag>::Payload;
            auto binding = source.binding();
            const auto range = typed_buffer_range<Payload>(binding.size(), first, count);
            return VSL_NAMESPACE::typed::TypedResourceBufferView<PayloadTag, kind>::FromBuffer(binding.native(), range.count, range.offset, binding.resource_use());
        }
    } // namespace detail

    template<typename PayloadTag, gpu_buffer Source>
    requires(detail::typed_buffer_usage<Source, VSL_NAMESPACE::MemoryType::UNIFORM_BUFFER>())
    auto uniform_view(const Source& source, size_t first, std::optional<size_t> count) {
        return detail::make_resource_view<PayloadTag, VSL_NAMESPACE::graphic_resource::Type::UNIFORM_BUFFER>(source, first, count);
    }
    template<typename PayloadTag, gpu_buffer Source>
    requires(detail::typed_buffer_usage<Source, VSL_NAMESPACE::MemoryType::STORAGE_BUFFER>())
    auto storage_view(const Source& source, size_t first, std::optional<size_t> count) {
        return detail::make_resource_view<PayloadTag, VSL_NAMESPACE::graphic_resource::Type::STORAGE_BUFFER>(source, first, count);
    }
    template<typename Vertex, gpu_buffer Source>
    requires(detail::typed_buffer_usage<Source, VSL_NAMESPACE::MemoryType::VERTEX_BUFFER>())
    auto vertex_view(const Source& source, size_t first, std::optional<size_t> count) {
        auto binding = source.binding();
        const auto range = detail::typed_buffer_range<Vertex>(binding.size(), first, count);
        return VSL_NAMESPACE::typed::TypedVertexBufferView<Vertex>::FromBuffer(binding.native(), range.count, range.offset, binding.resource_use());
    }
} // namespace AHO_NAMESPACE::resource
