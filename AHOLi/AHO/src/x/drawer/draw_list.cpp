#include <AHO/x/drawer/draw_list.hpp>

namespace AHO_NAMESPACE::x {
    std::array<float, 2> Transform2D::apply(float px, float py) const {
        return {xx * px + xy * py + x, yx * px + yy * py + y};
    }

    void DrawList::reserve(std::size_t vertices, std::size_t batches) {
        if (vertices > UINT32_MAX)
            throw std::length_error("DrawList exceeds uint32 vertex count");
        _vertices.reserve(vertices);
        _batches.reserve(batches);
    }

    void DrawList::clear() noexcept {
        _vertices.clear();
        _batches.clear();
        _transform = {};
    }

    bool DrawList::empty() const noexcept { return _vertices.empty(); }

    std::span<const DrawVertex> DrawList::vertices() const noexcept { return _vertices; }

    std::span<const DrawBatch> DrawList::batches() const noexcept { return _batches; }

    const Transform2D& DrawList::transform() const noexcept { return _transform; }

    void DrawList::setTransform(Transform2D value) {
        for (const auto f : {value.xx, value.xy, value.yx, value.yy, value.x, value.y})
            if (!std::isfinite(f))
                throw std::invalid_argument("Draw transform requires finite values");
        _transform = value;
    }

    void DrawList::triangle(const Triangle<float, VSL_NAMESPACE::D2>& shape, const RGBA& color) {
        const auto points = shape.points();
        const std::array<DrawVertex, 3> vertices{
            vertex(points[0].value.___AN1, points[0].value.___AN2, 0, 0, color),
            vertex(points[1].value.___AN1, points[1].value.___AN2, 0, 0, color),
            vertex(points[2].value.___AN1, points[2].value.___AN2, 0, 0, color)};
        append(vertices, {});
    }

    void DrawList::rectangle(DrawRect rect, const RGBA& color, TextureId texture, DrawRect uv) {
        const auto a = vertex(rect.x, rect.y, uv.x, uv.y, color);
        const auto b = vertex(rect.x + rect.width, rect.y, uv.x + uv.width, uv.y, color);
        const auto c = vertex(rect.x + rect.width, rect.y + rect.height, uv.x + uv.width, uv.y + uv.height, color);
        const auto d = vertex(rect.x, rect.y + rect.height, uv.x, uv.y + uv.height, color);
        const std::array vertices{a, b, c, a, c, d};
        append(vertices, texture);
    }

    DrawVertex DrawList::vertex(float x, float y, float u, float v, const RGBA& color) const {
        DrawVertex result{_transform.apply(x, y), {u, v}, {color.r.value, color.g.value, color.b.value, color.a.value}};
        for (const auto f : {x, y, u, v, result.position[0], result.position[1], color.r.value, color.g.value, color.b.value, color.a.value})
            if (!std::isfinite(f))
                throw std::invalid_argument("Draw vertices require finite values");
        return result;
    }

    void DrawList::append(std::span<const DrawVertex> vertices, TextureId texture) {
        if (vertices.empty())
            return;
        if (vertices.size() > UINT32_MAX - _vertices.size())
            throw std::length_error("DrawList exceeds uint32 vertex count");
        const auto first = static_cast<std::uint32_t>(_vertices.size());
        const auto count = static_cast<std::uint32_t>(vertices.size());
        const bool merge = !_batches.empty() && _batches.back().texture == texture;
        // 確保失敗時に頂点だけが追加された状態にならないよう、先に両方の容量を確保する。
        if (!merge && _batches.size() == _batches.capacity())
            _batches.reserve(std::min(_batches.max_size(), std::max(std::size_t{1}, _batches.capacity() * 2)));
        _vertices.insert(_vertices.end(), vertices.begin(), vertices.end());
        if (merge)
            _batches.back().vertexCount += count;
        else
            _batches.push_back({texture, first, count});
    }
} // namespace AHO_NAMESPACE::x
