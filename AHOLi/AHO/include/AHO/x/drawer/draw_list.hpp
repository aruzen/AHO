#pragma once
#include "../../core/color.hpp"
#include "../../core/triangle.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <compare>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace AHO_NAMESPACE::x {
    struct DrawRect {
        float x = 0, y = 0, width = 0, height = 0;
    };

    struct Transform2D {
        float xx = 1, xy = 0, yx = 0, yy = 1, x = 0, y = 0;
        [[nodiscard]] std::array<float, 2> apply(float px, float py) const;
    };

    struct DrawVertex {
        std::array<float, 2> position;
        std::array<float, 2> uv;
        std::array<float, 4> color;
    };

    // 0は単色。1以降はRendererへ登録したテクスチャを指す。
    struct TextureId {
        std::uint32_t value = 0;
        constexpr auto operator<=>(const TextureId&) const = default;
    };

    struct DrawBatch {
        TextureId texture;
        std::uint32_t first_vertex, vertex_count;
    };

    // CPUで再利用する描画リスト。追加順を保ち、隣り合う同一materialだけをまとめる。
    class DrawList {
    public:
        void reserve(std::size_t vertices, std::size_t batches = 1);
        void clear() noexcept;
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] std::span<const DrawVertex> vertices() const noexcept;
        [[nodiscard]] std::span<const DrawBatch> batches() const noexcept;
        [[nodiscard]] const Transform2D& transform() const noexcept;
        void set_transform(Transform2D value);
        void triangle(const Triangle<float, VSL_NAMESPACE::D2>& shape, const RGBA& color);
        void rectangle(DrawRect rect, const RGBA& color, TextureId texture = {}, DrawRect uv = {0, 0, 1, 1});
        // Polygonなどの既存の三角形ビューを、実装型に依存せず追加できる。
        template<AHO_NAMESPACE::concepts::is_triangles_object Shape>
        requires std::same_as<typename Shape::dimension, VSL_NAMESPACE::D2> &&
                 std::same_as<typename Shape::element_type, float> &&
                 std::same_as<typename Shape::coordinate_info, typename Triangle<float, VSL_NAMESPACE::D2>::coordinate_info>
        void geometry(const Shape& shape, const RGBA& color);

    private:
        std::vector<DrawVertex> _vertices;
        std::vector<DrawBatch> _batches;
        Transform2D _transform;
        DrawVertex vertex(float x, float y, float u, float v, const RGBA& color) const;
        void append(std::span<const DrawVertex> vertices, TextureId texture);
    };

    template<AHO_NAMESPACE::concepts::is_triangles_object Shape>
    requires std::same_as<typename Shape::dimension, VSL_NAMESPACE::D2> &&
             std::same_as<typename Shape::element_type, float> &&
             std::same_as<typename Shape::coordinate_info, typename Triangle<float, VSL_NAMESPACE::D2>::coordinate_info>
    void DrawList::geometry(const Shape& shape, const RGBA& color) {
        DrawList staged;
        staged._transform = _transform;
        for (const auto& t : shape.triangles())
            staged.triangle(t, color);
        append(staged.vertices(), {});
    }
} // namespace AHO_NAMESPACE::x
