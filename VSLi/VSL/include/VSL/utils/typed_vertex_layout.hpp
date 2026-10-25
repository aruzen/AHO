#pragma once
#include "../vulkan/stages/vertex_input.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace VSL_NAMESPACE::typed {
    inline constexpr std::uint32_t AUTO = UINT32_MAX;
    using VertexTiming = pipeline_layout::VertexInputShapeDefinition::UpdateTiming;

    template<data_format::Format format, std::uint32_t location = AUTO, std::uint32_t offset = AUTO>
    struct VertexInputDefinition {
        static constexpr pipeline_layout::VertexInputLayoutDefinition VALUE{format, location, offset};
    };

    template<std::uint32_t binding, std::uint32_t size, VertexTiming timing, typename... Attributes>
    struct VertexShapeDefinition {
        static constexpr std::uint32_t BINDING = binding, STRIDE = size;
        static constexpr VertexTiming TIMING = timing;
        static constexpr std::array<pipeline_layout::VertexInputLayoutDefinition, sizeof...(Attributes)> ATTRIBUTES{Attributes::VALUE...};
        static constexpr size_t Size() { return sizeof...(Attributes); }
        template<size_t index>
        static constexpr auto Get() { return std::get<index>(ATTRIBUTES); }
        static pipeline_layout::VertexInputShapeDefinition Make() {
            return {.layouts = {ATTRIBUTES.begin(), ATTRIBUTES.end()}, .binding = binding, .size = size, .update_timing = timing};
        }
    };

    // Associates a C++ record type with a binding; explicit offsets should use offsetof(Vertex, member).
    template<typename Vertex_, std::uint32_t binding, VertexTiming timing, typename... Attributes>
    struct VertexBinding : VertexShapeDefinition<binding, sizeof(Vertex_), timing, Attributes...> {
        using Vertex = Vertex_;
        static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>,
                      "Typed vertex records must be standard-layout and trivially copyable");
        static_assert(sizeof(Vertex) < AUTO, "Typed vertex record stride exceeds uint32 range");
        static_assert(((Attributes::VALUE.offset != AUTO) && ...),
                      "C++ vertex records require explicit offsetof values; automatic offsets describe packed data");
    };

    struct VertexBindingLayout {
        std::uint32_t binding, stride;
        VertexTiming timing;
    };
    struct VertexAttributeLayout {
        data_format::Format format = data_format::UNDEFINED;
        std::uint32_t location, offset, binding, locations;
    };

    namespace detail {
        template<size_t bindings, size_t attributes>
        struct VertexLayoutData {
            std::array<VertexBindingLayout, bindings> binding{};
            std::array<VertexAttributeLayout, attributes> attribute{};
            bool valid = true;
        };

        template<typename T>
        struct VertexMemberFormat {
            static constexpr auto VALUE = [] {
                if constexpr (std::is_same_v<std::remove_cv_t<T>, signed char>)
                    return data_format::INT8;
                else
                    return data_format::convert_graphic_type<std::remove_cv_t<T>>();
            }();
        };

        template<typename T, size_t count>
        struct VertexMemberFormat<T[count]> {
            static constexpr auto VALUE = [] {
                constexpr auto scalar = VertexMemberFormat<T>::VALUE;
                if constexpr (scalar.size() == sizeof(T) && scalar.get_vec_length() == 1)
                    return scalar.template to_vec<count>();
                else
                    return data_format::UNDEFINED;
            }();
        };

        template<typename T, size_t count>
        struct VertexMemberFormat<std::array<T, count>> : VertexMemberFormat<T[count]> {};

        template<typename Record, std::uint32_t binding, VertexTiming timing, std::uint32_t first_location, typename Indices>
        struct RecordBinding;

        template<typename Record, std::uint32_t binding, VertexTiming timing, std::uint32_t first_location, size_t... indices>
        struct RecordBinding<Record, binding, timing, first_location, std::index_sequence<indices...>> {
            static_assert(((std::uint64_t(first_location) + Record::ATTRIBUTES[indices].location +
                                Record::ATTRIBUTES[indices].locations <=
                            AUTO) &&
                           ...),
                          "Vertex record location overflow");
            using Type = VertexBinding<typename Record::Vertex, binding, timing,
                                       VertexInputDefinition<Record::ATTRIBUTES[indices].format, first_location + Record::ATTRIBUTES[indices].location,
                                                             Record::ATTRIBUTES[indices].offset>...>;
        };

        constexpr std::uint32_t vertex_locations(data_format::Format format) {
            using enum data_format::FormatTypes;
            switch (format.type()) {
                case UNSIGNED_INT64_RGB:
                case SIGNED_INT64_RGB:
                case SIGNED_FLOAT64_RGB:
                case UNSIGNED_INT64_RGBA:
                case SIGNED_INT64_RGBA:
                case SIGNED_FLOAT64_RGBA:
                    return 2;
                default:
                    return 1;
            }
        }

        template<typename... Shapes>
        consteval auto resolve_vertex_layout() {
            VertexLayoutData<sizeof...(Shapes), (Shapes::ATTRIBUTES.size() + ... + 0)> result;
            std::uint64_t next_binding = 0, next_location = 0;
            size_t binding_index = 0, attribute_index = 0;
            auto shape = [&]<typename Shape>() {
                const std::uint64_t binding = Shape::BINDING == AUTO ? next_binding : Shape::BINDING;
                if (binding >= AUTO || (Shape::TIMING != VertexTiming::NEXT_VERTEX && Shape::TIMING != VertexTiming::NEXT_INSTANCE))
                    result.valid = false;
                for (size_t i = 0; i < binding_index; ++i)
                    if (result.binding[i].binding == binding)
                        result.valid = false;
                next_binding = binding + 1;
                std::uint64_t next_offset = 0, extent = 0;
                for (auto attribute : Shape::ATTRIBUTES) {
                    const std::uint64_t location = attribute.location == AUTO ? next_location : attribute.location;
                    const std::uint64_t offset = attribute.offset == AUTO ? next_offset : attribute.offset;
                    const auto locations = vertex_locations(attribute.format);
                    if (!attribute.format.size() || attribute.format.type() == data_format::FormatTypes::UNDEFINED ||
                        location + locations > AUTO || offset + attribute.format.size() > AUTO)
                        result.valid = false;
                    for (size_t i = 0; i < attribute_index; ++i) {
                        const auto& prior = result.attribute[i];
                        if (location < std::uint64_t(prior.location) + prior.locations && prior.location < location + locations)
                            result.valid = false;
                    }
                    result.attribute[attribute_index++] = {attribute.format, std::uint32_t(location), std::uint32_t(offset), std::uint32_t(binding), locations};
                    next_location = location + locations;
                    next_offset = offset + attribute.format.size();
                    if (extent < next_offset)
                        extent = next_offset;
                }
                const std::uint64_t stride = Shape::STRIDE == AUTO ? extent : Shape::STRIDE;
                if (stride > AUTO || stride < extent)
                    result.valid = false;
                result.binding[binding_index++] = {std::uint32_t(binding), std::uint32_t(stride), Shape::TIMING};
            };
            (shape.template operator()<Shapes>(), ...);
            return result;
        }
    } // namespace detail

    template<typename... Shapes_>
    struct TypedVertexInput {
        using Shapes = std::tuple<Shapes_...>;
        static constexpr auto LAYOUT = detail::resolve_vertex_layout<Shapes_...>();
        static_assert(LAYOUT.valid, "Invalid typed vertex layout: duplicate binding/location, format, extent or stride");
        static constexpr size_t BINDING_COUNT = sizeof...(Shapes_);
        template<size_t index>
        using Shape = std::tuple_element_t<index, Shapes>;

        void injection(VSL_NAMESPACE::PipelineLayoutEditAccessor layout) const;
    };

    // Keep the member's actual type and offsetof together; a format override expresses e.g. UNORM storage.
    template<typename Vertex_, typename Member, std::uint32_t offset, std::uint32_t location = AUTO,
             data_format::Format format = detail::VertexMemberFormat<std::remove_cv_t<Member>>::VALUE>
    struct VertexMember : VertexInputDefinition<format, location, offset> {
        using Vertex = Vertex_;
        static_assert(format.size() != 0 && format.size() <= sizeof(Member), "Vertex member format exceeds its C++ storage or is undefined");
        static_assert(offset <= sizeof(Vertex) && sizeof(Member) <= sizeof(Vertex) - offset, "Vertex member exceeds its record");
    };

    template<typename Vertex_, typename... Members>
    struct VertexRecord {
        using Vertex = Vertex_;
        static_assert((std::is_same_v<Vertex, typename Members::Vertex> && ...), "Vertex member belongs to a different record");
        static constexpr auto LAYOUT = TypedVertexInput<VertexBinding<Vertex, 0, VertexTiming::NEXT_VERTEX, Members...>>::LAYOUT;
        static constexpr auto ATTRIBUTES = LAYOUT.attribute;
        template<std::uint32_t binding, VertexTiming timing = VertexTiming::NEXT_VERTEX, std::uint32_t first_location = 0>
        using Binding = typename detail::RecordBinding<VertexRecord, binding, timing, first_location,
                                                       std::make_index_sequence<sizeof...(Members)>>::Type;
    };

    template<typename... Shapes_>
    void TypedVertexInput<Shapes_...>::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor layout) const {
        // All automatic values are already resolved. Reuse the backend's owned Vulkan metadata.
        pipeline_layout::VertexInput input;
        input.definitions.reserve(BINDING_COUNT);
        for (const auto& binding : LAYOUT.binding) {
            pipeline_layout::VertexInputShapeDefinition shape;
            shape.binding = binding.binding;
            shape.size = binding.stride;
            shape.update_timing = binding.timing;
            for (const auto& attribute : LAYOUT.attribute)
                if (attribute.binding == binding.binding)
                    shape.layouts.push_back({attribute.format, attribute.location, attribute.offset});
            input.definitions.push_back(std::move(shape));
        }
        input.injection(layout);
    }

} // namespace VSL_NAMESPACE::typed

// コンパイラー間で使える型とoffsetofだけを取得し、layoutの解決と検証は上のtemplateで行う。
// 省略可能な引数はlocationとformat。
#define VSL_VERTEX_MEMBER(Record, member, ...) \
    ::VSL_NAMESPACE::typed::VertexMember<Record, decltype(Record::member), offsetof(Record, member) __VA_OPT__(, ) __VA_ARGS__>
