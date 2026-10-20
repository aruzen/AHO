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
        [[nodiscard]] std::array<float, 2> apply(float px, float py) const {
            return {xx * px + xy * py + x, yx * px + yy * py + y};
        }
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
        std::uint32_t firstVertex, vertexCount;
    };
    // CPUで再利用する描画リスト。追加順を保ち、隣り合う同一materialだけをまとめる。
    class DrawList {
    public:
        void reserve(std::size_t vertices, std::size_t batches = 1) {
            if (vertices > UINT32_MAX)
                throw std::length_error("DrawList exceeds uint32 vertex count");
            vertices_.reserve(vertices);
            batches_.reserve(batches);
        }
        void clear() noexcept {
            vertices_.clear();
            batches_.clear();
            transform_ = {};
        }
        [[nodiscard]] bool empty() const noexcept { return vertices_.empty(); }
        [[nodiscard]] std::span<const DrawVertex> vertices() const noexcept { return vertices_; }
        [[nodiscard]] std::span<const DrawBatch> batches() const noexcept { return batches_; }
        [[nodiscard]] const Transform2D& transform() const noexcept { return transform_; }
        void setTransform(Transform2D value) {
            for (const auto f : {value.xx, value.xy, value.yx, value.yy, value.x, value.y})
                if (!std::isfinite(f))
                    throw std::invalid_argument("Draw transform requires finite values");
            transform_ = value;
        }
        void triangle(const Triangle<float, VSL_NAMESPACE::D2>& shape, const RGBA& color) {
            const auto points = shape.points();
            const std::array<DrawVertex, 3> vertices{
                vertex(points[0].value.___AN1, points[0].value.___AN2, 0, 0, color),
                vertex(points[1].value.___AN1, points[1].value.___AN2, 0, 0, color),
                vertex(points[2].value.___AN1, points[2].value.___AN2, 0, 0, color)};
            append(vertices, {});
        }
        void rectangle(DrawRect rect, const RGBA& color, TextureId texture = {}, DrawRect uv = {0, 0, 1, 1}) {
            const auto a = vertex(rect.x, rect.y, uv.x, uv.y, color);
            const auto b = vertex(rect.x + rect.width, rect.y, uv.x + uv.width, uv.y, color);
            const auto c = vertex(rect.x + rect.width, rect.y + rect.height, uv.x + uv.width, uv.y + uv.height, color);
            const auto d = vertex(rect.x, rect.y + rect.height, uv.x, uv.y + uv.height, color);
            const std::array vertices{a, b, c, a, c, d};
            append(vertices, texture);
        }
        // Polygonなどの既存の三角形ビューを、実装型に依存せず追加できる。
        template<AHO_NAMESPACE::concepts::is_triangles_object Shape>
        requires std::same_as<typename Shape::dimension, VSL_NAMESPACE::D2> &&
                 std::same_as<typename Shape::element_type, float> &&
                 std::same_as<typename Shape::coordinate_info, typename Triangle<float, VSL_NAMESPACE::D2>::coordinate_info>
        void geometry(const Shape& shape, const RGBA& color) {
            DrawList staged;
            staged.transform_ = transform_;
            for (const auto& t : shape.triangles())
                staged.triangle(t, color);
            append(staged.vertices(), {});
        }

    private:
        std::vector<DrawVertex> vertices_;
        std::vector<DrawBatch> batches_;
        Transform2D transform_;
        DrawVertex vertex(float x, float y, float u, float v, const RGBA& color) const {
            DrawVertex result{transform_.apply(x, y), {u, v}, {color.r.value, color.g.value, color.b.value, color.a.value}};
            for (const auto f : {x, y, u, v, result.position[0], result.position[1], color.r.value, color.g.value, color.b.value, color.a.value})
                if (!std::isfinite(f))
                    throw std::invalid_argument("Draw vertices require finite values");
            return result;
        }
        void append(std::span<const DrawVertex> vertices, TextureId texture) {
            if (vertices.empty())
                return;
            if (vertices.size() > UINT32_MAX - vertices_.size())
                throw std::length_error("DrawList exceeds uint32 vertex count");
            const auto first = static_cast<std::uint32_t>(vertices_.size());
            const auto count = static_cast<std::uint32_t>(vertices.size());
            const bool merge = !batches_.empty() && batches_.back().texture == texture;
            // 確保失敗時に頂点だけが追加された状態にならないよう、先に両方の容量を確保する。
            if (!merge && batches_.size() == batches_.capacity())
                batches_.reserve(std::min(batches_.max_size(), std::max(std::size_t{1}, batches_.capacity() * 2)));
            vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
            if (merge)
                batches_.back().vertexCount += count;
            else
                batches_.push_back({texture, first, count});
        }
    };
} // namespace AHO_NAMESPACE::x
