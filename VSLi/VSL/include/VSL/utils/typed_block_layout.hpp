#pragma once
#include "typed_vertex_layout.hpp"
#include <limits>
#include <span>
#include <concepts>

namespace VSL_NAMESPACE::typed {
    enum class BlockKind {
        UNKNOWN,
        SCALAR,
        VECTOR,
        MATRIX,
        ARRAY,
        STRUCTURE
    };
    enum class BlockNumeric {
        UNKNOWN,
        FLOATING_POINT,
        SIGNED_INTEGER,
        UNSIGNED_INTEGER
    };
    struct BlockType;
    struct BlockField {
        size_t offset;
        const BlockType* type;
    };
    struct BlockType {
        BlockKind kind = BlockKind::UNKNOWN;
        BlockNumeric numeric = BlockNumeric::UNKNOWN;
        std::uint32_t width = 0, components = 1, columns = 1;
        size_t size = 0, count = 0, stride = 0;
        bool row_major = false;
        const BlockType* element = nullptr;
        std::span<const BlockField> fields;
    };
    namespace detail {
        constexpr bool block_layout_known(const BlockType& type);
        consteval size_t block_extent(size_t count, size_t stride, size_t size);
        template<typename T>
        consteval BlockType infer_block_layout();
    } // namespace detail
    template<typename T>
    struct BlockLayout {
        static constexpr auto LAYOUT = detail::infer_block_layout<std::remove_cv_t<T>>();
    };
    template<>
    struct BlockLayout<void> {
        static constexpr BlockType LAYOUT{};
    };
    template<typename T>
    struct PayloadLayout {
        using Payload = T;
        static constexpr auto LAYOUT = BlockLayout<T>::LAYOUT;
    };
    template<typename T>
    requires requires { typename T::Record; T::LAYOUT; }
    struct PayloadLayout<T> {
        using Payload = typename T::Record;
        static constexpr auto LAYOUT = T::LAYOUT;
    };
    template<typename Scalar, size_t components>
    struct BlockVector {
        static_assert(components > 0 && components <= 4 &&
                      BlockLayout<Scalar>::LAYOUT.kind == BlockKind::SCALAR &&
                      BlockLayout<Scalar>::LAYOUT.size == sizeof(Scalar));
        static constexpr BlockType LAYOUT{
            .kind = BlockKind::VECTOR,
            .numeric = BlockLayout<Scalar>::LAYOUT.numeric,
            .width = BlockLayout<Scalar>::LAYOUT.width,
            .components = components,
            .size = components * sizeof(Scalar)};
    };
    template<typename Scalar, size_t columns, size_t rows, size_t stride = sizeof(Scalar) * rows, bool row_major = false>
    struct BlockMatrix {
        static_assert(columns >= 2 && columns <= 4 && rows >= 2 && rows <= 4 &&
                          BlockLayout<Scalar>::LAYOUT.kind == BlockKind::SCALAR &&
                          BlockLayout<Scalar>::LAYOUT.size == sizeof(Scalar) &&
                          BlockLayout<Scalar>::LAYOUT.numeric == BlockNumeric::FLOATING_POINT,
                      "Typed block matrices require floating-point components and 2..4 dimensions");
        static constexpr BlockType LAYOUT{
            .kind = BlockKind::MATRIX,
            .numeric = BlockLayout<Scalar>::LAYOUT.numeric,
            .width = BlockLayout<Scalar>::LAYOUT.width,
            .components = rows,
            .columns = columns,
            .size = detail::block_extent(row_major ? rows : columns, stride, sizeof(Scalar) * (row_major ? columns : rows)),
            .stride = stride,
            .row_major = row_major};
    };
    template<typename Element, size_t count, size_t stride = sizeof(typename PayloadLayout<Element>::Payload)>
    struct BlockArray {
        static_assert(count > 0, "Typed block arrays require a nonzero C++ capacity");
        static constexpr BlockType LAYOUT{.kind = BlockKind::ARRAY,
                                          .size = detail::block_extent(count, stride, PayloadLayout<Element>::LAYOUT.size),
                                          .count = count,
                                          .stride = stride,
                                          .element = &PayloadLayout<Element>::LAYOUT};
    };
    template<typename T, size_t count>
    struct BlockLayout<T[count]> {
        static consteval BlockType MakeLayout();
        static constexpr auto LAYOUT = MakeLayout();
    };
    template<typename T, size_t count>
    struct BlockLayout<std::array<T, count>> : BlockArray<T, count> {};

    template<typename Record_, typename Member, size_t offset, typename Policy = BlockLayout<std::remove_cv_t<Member>>>
    struct BlockMember {
        using Record = Record_;
        static_assert(detail::block_layout_known(Policy::LAYOUT), "Typed block member requires an explicit BlockRecord/layout");
        static_assert(Policy::LAYOUT.size <= sizeof(Member) && offset <= sizeof(Record) && sizeof(Member) <= sizeof(Record) - offset,
                      "Typed block member layout exceeds C++ storage");
        static constexpr BlockField FIELD{offset, &Policy::LAYOUT};
    };
    template<typename Record_, typename... Members>
    struct BlockRecord {
        using Record = Record_;
        static_assert(std::is_trivially_copyable_v<Record> && std::is_standard_layout_v<Record>, "Typed block record must be trivial and standard-layout");
        static_assert((std::same_as<Record, typename Members::Record> && ...), "Typed block member belongs to a different record");
        static constexpr std::array<BlockField, sizeof...(Members)> FIELDS{Members::FIELD...};
        static constexpr BlockType LAYOUT{
            .kind = BlockKind::STRUCTURE,
            .size = sizeof(Record),
            .fields = FIELDS};
    };
    template<typename Record_, typename Policy>
    struct BlockValue {
        using Record = Record_;
        static_assert(std::is_trivially_copyable_v<Record> && std::is_standard_layout_v<Record> && Policy::LAYOUT.size <= sizeof(Record),
                      "Typed block value layout exceeds its trivial C++ record");
        static constexpr auto LAYOUT = Policy::LAYOUT;
    };

    namespace detail {
        constexpr bool block_layout_known(const BlockType& type) {
            if (type.kind == BlockKind::UNKNOWN)
                return false;
            if (type.kind == BlockKind::ARRAY)
                return type.element && block_layout_known(*type.element);
            for (const auto& field : type.fields)
                if (!field.type || !block_layout_known(*field.type))
                    return false;
            return true;
        }
        consteval size_t block_extent(size_t count, size_t stride, size_t size) {
            if (!count || !stride || stride < size || (count - 1) > (std::numeric_limits<size_t>::max() - size) / stride)
                throw "Typed block layout extent overflow or invalid stride";
            return (count - 1) * stride + size;
        }
        template<typename T>
        consteval BlockType infer_block_layout() {
            if constexpr (std::is_enum_v<T>)
                return BlockLayout<std::underlying_type_t<T>>::LAYOUT;
            else {
                constexpr auto format = VertexMemberFormat<T>::VALUE;
                if constexpr (format == data_format::UNDEFINED)
                    return {.size = sizeof(T)};
                else {
                    constexpr auto scalar = format.template to_vec<1>();
                    BlockNumeric numeric = BlockNumeric::UNKNOWN;
                    using enum data_format::FormatTypes;
                    switch (scalar.type()) {
                        case SIGNED_FLOAT16_R:
                        case SIGNED_FLOAT32_R:
                        case SIGNED_FLOAT64_R:
                            numeric = BlockNumeric::FLOATING_POINT;
                            break;
                        case SIGNED_INT8_R:
                        case SIGNED_INT16_R:
                        case SIGNED_INT32_R:
                        case SIGNED_INT64_R:
                        case SIGNED_NORMALIZED8_R:
                        case SIGNED_SCALED8_R:
                        case SIGNED_NORMALIZED16_R:
                        case SIGNED_SCALED16_R:
                            numeric = BlockNumeric::SIGNED_INTEGER;
                            break;
                        default:
                            numeric = BlockNumeric::UNSIGNED_INTEGER;
                            break;
                    }
                    return {
                        .kind = format.get_vec_length() == 1 ? BlockKind::SCALAR : BlockKind::VECTOR,
                        .numeric = numeric,
                        .width = scalar.size() * 8,
                        .components = std::uint32_t(format.get_vec_length()),
                        .size = format.size()};
                }
            }
        }
    } // namespace detail
    template<typename T, size_t count>
    consteval BlockType BlockLayout<T[count]>::MakeLayout() {
        if constexpr (count > 0 && count <= 4 && BlockLayout<T>::LAYOUT.kind == BlockKind::SCALAR && BlockLayout<T>::LAYOUT.size == sizeof(T))
            return BlockVector<T, count>::LAYOUT;
        else
            return BlockArray<T, count>::LAYOUT;
    }
} // namespace VSL_NAMESPACE::typed

#define VSL_BLOCK_MEMBER(Record, member, ...) \
    ::VSL_NAMESPACE::typed::BlockMember<Record, decltype(Record::member), offsetof(Record, member) __VA_OPT__(, ) __VA_ARGS__>
