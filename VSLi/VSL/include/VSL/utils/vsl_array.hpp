#pragma once
#include "../define.hpp"
#include "string.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <typeinfo>

namespace VSL_NAMESPACE {
    template<typename T>
    concept convert_to_vsl_array = requires {
        typename T::ElementType;
    };

    template<convert_to_vsl_array T>
    constexpr size_t vsl_array_size();

    template<typename T>
    concept has_point_index = requires {
        { T::POINT_INDEX } -> std::convertible_to<size_t>;
        requires(!std::is_member_object_pointer_v<decltype(&T::POINT_INDEX)>);
    };

    template<typename T>
    concept has_size_index = requires {
        { T::SIZE_INDEX } -> std::convertible_to<size_t>;
        requires(!std::is_member_object_pointer_v<decltype(&T::SIZE_INDEX)>);
    };

    template<typename T>
    concept has_rgb_color_index = requires {
        { T::COLOR_INDEX } -> std::convertible_to<size_t>;
        requires(!std::is_member_object_pointer_v<decltype(&T::COLOR_INDEX)>);
    };

    template<typename T, size_t size>
    concept convert_to_vsl_sized_array = convert_to_vsl_array<T> && size == vsl_array_size<T>();

    template<typename T, size_t size>
    concept convert_to_vsl_at_least_array = convert_to_vsl_array<T> && size <= vsl_array_size<T>();

    template<convert_to_vsl_array T, typename ElementType_ = T::ElementType, size_t size = vsl_array_size<T>()>
    struct VSLArray {
        using ElementType = ElementType_;

        std::array<ElementType_, size>* ptr;

        constexpr VSLArray(T* t) {
            ptr = reinterpret_cast<std::array<ElementType_, size>*>(t);
        }

        ElementType_& operator[](size_t t) {
            return (*ptr)[t];
        }
    };

    template<typename ElementType_>
    struct VSLD2PointDefault {
        using ElementType = ElementType_;
        ElementType_ x, y;
    };

    template<typename ElementType_>
    struct VSLD2PointAccessor {
        using ElementType = ElementType_;
        ElementType_ &x, &y;
    };

    template<typename T>
    struct VSLD2Point {
        static constexpr int SPECIALIZED_ID = 0;
    };

    template<typename T>
    concept is_point = requires(T t) {
        typename T::ElementType;
        { t.x } -> std::convertible_to<typename T::ElementType&>;
        { t.y } -> std::convertible_to<typename T::ElementType&>;
    };

    template<typename T>
    requires is_point<T>
    struct VSLD2Point<T> : public VSLD2PointAccessor<typename T::ElementType> {
        static constexpr int SPECIALIZED_ID = 1;

        using ElementType = T::ElementType;

        VSLD2Point(T* t) : VSLD2PointAccessor<typename T::ElementType>{t->x, t->y} {
            if constexpr (VSL_NAMESPACE::VALIDATION) {
                VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by is_point)");
            }
        }
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 2> && (!has_point_index<T>) && (!is_point<T>)
    struct VSLD2Point<T> : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLD2PointAccessor<typename T::ElementType> {
        static constexpr int SPECIALIZED_ID = 2;

        using ElementType = T::ElementType;

        VSLD2Point(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t), VSLD2PointAccessor<typename T::ElementType>{(*this)[0], (*this)[1]} {
            if constexpr (VSL_NAMESPACE::VALIDATION) {
                VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array)");
            }
        }
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 2> && has_point_index<T> && (!is_point<T>)
    struct VSLD2Point<T> : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLD2PointAccessor<typename T::ElementType> {
        static constexpr int SPECIALIZED_ID = 3;

        using ElementType = T::ElementType;

        VSLD2Point(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t),
                           VSLD2PointAccessor<typename T::ElementType>{(*this)[T::POINT_INDEX], (*this)[T::POINT_INDEX + 1]} {
            if constexpr (VSL_NAMESPACE::VALIDATION) {
                VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array & has_index)");
            }
        }
    };

    template<typename ElementType_>
    struct VSLD2SizeDefault {
        using ElementType = ElementType_;
        ElementType_ width, height;
    };

    template<typename ElementType_>
    struct VSLD2SizeAccessor {
        using ElementType = ElementType_;
        ElementType_ &width, &height;
    };

    template<typename T>
    struct VSLD2Size {};

    template<typename T>
    concept is_size = requires(T t) {
        typename T::ElementType;
        { t.height } -> std::convertible_to<typename T::ElementType&>;
        { t.width } -> std::convertible_to<typename T::ElementType&>;
    };

    template<typename T>
    requires is_size<T>
    struct VSLD2Size<T> : public VSLD2SizeAccessor<typename T::ElementType> {
        static constexpr int SPECIALIZED_ID = 1;
        using ElementType = T::ElementType;

        VSLD2Size(T* t) : VSLD2SizeAccessor<typename T::ElementType>{t->width, t->height} {}
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 2> && (!has_size_index<T>) && (!is_size<T>)
    struct VSLD2Size<T> : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLD2SizeAccessor<typename T::ElementType> {
        static constexpr int SPECIALIZED_ID = 2;

        using ElementType = T::ElementType;

        VSLD2Size(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t),
                          VSLD2SizeAccessor<typename T::ElementType>{(*this)[0], (*this)[1]} {
            if constexpr (VSL_NAMESPACE::VALIDATION) {
                VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array)");
            }
        }
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 2> && has_size_index<T> && (!is_size<T>)
    struct VSLD2Size<T> : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLD2SizeAccessor<typename T::ElementType> {
        static constexpr int SPECIALIZED_ID = 3;

        using ElementType = T::ElementType;

        VSLD2Size(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t),
                          VSLD2SizeAccessor<typename T::ElementType>{(*this)[T::SIZE_INDEX], (*this)[T::SIZE_INDEX + 1]} {
            if constexpr (VSL_NAMESPACE::VALIDATION) {
                VSL_NAMESPACE::loggingln(typeid(*this).name(), " : ", typeid(T).name(), " (processed by vsl_array & has_index)");
            }
        }
    };

    template<typename ElementType_>
    struct VSLD2RectangleDefault : public VSLD2PointDefault<ElementType_>, public VSLD2SizeDefault<ElementType_> {
        using ElementType = ElementType_;
        static constexpr size_t SIZE_INDEX = 2;
    };

    template<typename ElementType_>
    struct VSLD2RectangleAccessor : public VSLD2PointAccessor<ElementType_>, public VSLD2SizeAccessor<ElementType_> {
        using ElementType = ElementType_;
    };

    template<typename T>
    struct VSLD2Rectangle {};

    template<typename T>
    concept is_rectangle = is_point<T> && is_size<T>;

    template<typename T>
    requires is_rectangle<T>
    struct VSLD2Rectangle<T> : public VSLD2RectangleAccessor<typename T::ElementType> {

        VSLD2Rectangle(T* t) : VSLD2RectangleAccessor<typename T::ElementType>{t->x, t->y, t->width, t->height} {}
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 4> && (!is_rectangle<T>)
    struct VSLD2Rectangle<T> : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLD2RectangleAccessor<typename T::ElementType> {
        using ElementType = T::ElementType;

        static constexpr size_t PointOffset() {
            if constexpr (has_point_index<T>)
                return T::POINT_INDEX;
            else
                return 0;
        }
        static constexpr size_t SizeOffset() {
            if constexpr (has_size_index<T>)
                return T::SIZE_INDEX;
            else
                return 2;
        }

        VSLD2Rectangle(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t),
                               VSLD2RectangleAccessor<typename T::ElementType>{
                                   (*this)[PointOffset()],
                                   (*this)[PointOffset() + 1],
                                   (*this)[SizeOffset()],
                                   (*this)[SizeOffset() + 1]} {}
    };

    template<typename ElementType_>
    struct VSLDRGBColorDefault {
        using ElementType = ElementType_;

        ElementType r, g, b;
    };

    template<typename ElementType_>
    struct VSLDRGBColorAccessor {
        using ElementType = ElementType_;

        ElementType &r, &g, &b;
    };

    template<typename T>
    struct VSLDRGBColor {};

    template<typename T>
    concept is_rgb = requires(T t) {
        typename T::ElementType;
        { t.r } -> std::convertible_to<typename T::ElementType&>;
        { t.g } -> std::convertible_to<typename T::ElementType&>;
        { t.b } -> std::convertible_to<typename T::ElementType&>;
    };

    template<typename T>
    requires is_rgb<T>
    struct VSLDRGBColor<T> : public VSLDRGBColorAccessor<typename T::ElementType> {
        using ElementType = T::ElementType;

        VSLDRGBColor(T* t) : VSLDRGBColorAccessor<typename T::ElementType>{t->r, t->g, t->b} {}
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 3> && (!has_rgb_color_index<T>) && (!is_rgb<T>)
    struct VSLDRGBColor<T>
        : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLDRGBColorAccessor<typename T::ElementType> {

        using ElementType = T::ElementType;

        VSLDRGBColor(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t),
                             VSLDRGBColorAccessor<typename T::ElementType>{(*this)[0], (*this)[1], (*this)[2]} {}
    };

    template<typename T>
    requires convert_to_vsl_at_least_array<T, 3> && has_rgb_color_index<T> && (!is_rgb<T>)
    struct VSLDRGBColor<T>
        : public VSLArray<T, typename T::ElementType, vsl_array_size<T>()>, public VSLDRGBColorAccessor<typename T::ElementType> {

        using ElementType = T::ElementType;

        VSLDRGBColor(T* t) : VSLArray<T, ElementType, vsl_array_size<T>()>(t),
                             VSLDRGBColorAccessor<typename T::ElementType>{(*this)[T::COLOR_INDEX + 0], (*this)[T::COLOR_INDEX + 1], (*this)[T::COLOR_INDEX + 2]} {}
    };

    template<convert_to_vsl_array T>
    constexpr size_t vsl_array_size() {
        return sizeof(T) / sizeof(typename T::ElementType);
    }
} // namespace VSL_NAMESPACE
