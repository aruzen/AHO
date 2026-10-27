#include <AHO/x/text/text_layout.hpp>
#include "utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace AHO_NAMESPACE::x::text {
    namespace {
        struct LayoutBuilder {
            std::vector<TextGeometry::PointType> points;
            std::vector<TextGeometry::TriangleType> triangles;
            std::vector<VertexAttributes> vertices;
            std::vector<TriangleAttributes> faces;
            std::vector<PositionedGlyph> glyphs;
            std::vector<TextDrawRange> ranges;
            TextMetrics metrics;
            void append_quad(const resource::MSDFGlyph& glyph, double x, double y, const TextOptions& options);
            void include_ink(const resource::MSDFGlyph& glyph, double x, double y, const TextOptions& options);
        };
        float checked_position(double value);
        std::size_t checked_product(std::size_t count, std::size_t stride);
        void validate_options(const TextOptions& options);

        float checked_position(double value) {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::overflow_error("Text position cannot be represented by the Mesh scalar type");
            return static_cast<float>(value);
        }

        std::size_t checked_product(std::size_t count, std::size_t stride) {
            if (count > std::numeric_limits<std::size_t>::max() / stride)
                throw std::length_error("Text buffer byte count overflow");
            return count * stride;
        }

        void validate_options(const TextOptions& o) {
            if (!std::isfinite(o.em_size) || o.em_size <= 0 || !std::isfinite(o.line_spacing) || o.line_spacing <= 0 ||
                !std::isfinite(o.letter_spacing) || !o.tab_spaces ||
                !std::isfinite(o.baseline.value.___AN1) || !std::isfinite(o.baseline.value.___AN2) ||
                (o.y_direction != ClipY::UP && o.y_direction != ClipY::DOWN))
                throw std::invalid_argument("Invalid text layout options");
        }

        void LayoutBuilder::include_ink(const resource::MSDFGlyph& glyph, double x, double y, const TextOptions& options) {
            const auto& b = glyph.ink_bounds;
            const double direction = options.y_direction == ClipY::UP ? 1 : -1;
            const double left = x + b.left * options.em_size, right = x + b.right * options.em_size;
            const auto first = y + direction * b.bottom * options.em_size, second = y + direction * b.top * options.em_size;
            const auto bottom = std::min(first, second), top = std::max(first, second);
            if (!metrics.has_ink) {
                metrics.minimum = {left, bottom};
                metrics.maximum = {right, top};
                metrics.has_ink = true;
            } else {
                metrics.minimum.value.___AN1 = std::min(metrics.minimum.value.___AN1, left);
                metrics.minimum.value.___AN2 = std::min(metrics.minimum.value.___AN2, bottom);
                metrics.maximum.value.___AN1 = std::max(metrics.maximum.value.___AN1, right);
                metrics.maximum.value.___AN2 = std::max(metrics.maximum.value.___AN2, top);
            }
        }

        void LayoutBuilder::append_quad(const resource::MSDFGlyph& glyph, double x, double y, const TextOptions& options) {
            if (points.size() > UINT32_MAX - 4)
                throw std::length_error("Text mesh exceeds uint32 vertex indices");
            const auto base = static_cast<std::uint32_t>(points.size());
            const auto& b = glyph.plane_bounds;
            const double direction = options.y_direction == ClipY::UP ? 1 : -1;
            const float left = checked_position(x + b.left * options.em_size), right = checked_position(x + b.right * options.em_size);
            const float top = checked_position(y + direction * b.top * options.em_size), bottom = checked_position(y + direction * b.bottom * options.em_size);
            points.emplace_back(left, top);
            points.emplace_back(right, top);
            points.emplace_back(right, bottom);
            points.emplace_back(left, bottom);
            const auto& uv = glyph.uv;
            vertices.insert(vertices.end(), {{{uv.left, uv.top}}, {{uv.right, uv.top}}, {{uv.right, uv.bottom}}, {{uv.left, uv.bottom}}});
            if (options.y_direction == ClipY::DOWN) {
                triangles.push_back({{base}, {base + 1}, {base + 2}});
                triangles.push_back({{base + 2}, {base + 3}, {base}});
            } else {
                triangles.push_back({{base}, {base + 2}, {base + 1}});
                triangles.push_back({{base + 2}, {base}, {base + 3}});
            }
            faces.push_back({glyph.page});
            faces.push_back({glyph.page});
            const auto first_index = (triangles.size() - 2) * 3;
            if (ranges.empty() || ranges.back().page != glyph.page)
                ranges.push_back({glyph.page, first_index, 6});
            else
                ranges.back().index_count += 6;
            include_ink(glyph, x, y, options);
        }
    } // namespace

    TextLayout::TextLayout(resource::MSDFAtlas atlas, TextMesh mesh, std::vector<PositionedGlyph> glyphs,
                           std::vector<TextDrawRange> ranges, TextMetrics metrics)
        : _atlas(std::move(atlas)), _mesh(std::move(mesh)), _glyphs(std::move(glyphs)), _ranges(std::move(ranges)), _metrics(metrics) {}

    const resource::MSDFAtlas& TextLayout::atlas() const {
        return _atlas;
    }
    const TextMesh& TextLayout::mesh() const {
        return _mesh;
    }
    std::span<const PositionedGlyph> TextLayout::glyphs() const {
        return _glyphs;
    }
    std::span<const TextDrawRange> TextLayout::ranges() const {
        return _ranges;
    }
    const TextMetrics& TextLayout::metrics() const {
        return _metrics;
    }

    TextLayout layout_text(resource::MSDFAtlas& atlas, std::string_view utf8, const TextOptions& options) {
        validate_options(options);
        const auto codepoints = detail::decode_utf8(utf8);
        const auto metrics = atlas.font().metrics();
        const double line_step = metrics.line_height * options.em_size * options.line_spacing;
        if (!std::isfinite(line_step) || line_step <= 0)
            throw std::overflow_error("Text line spacing cannot be represented");
        LayoutBuilder builder;
        double x = options.baseline.value.___AN1, y = options.baseline.value.___AN2;
        const auto start_x = x;
        builder.metrics.minimum = builder.metrics.maximum = builder.metrics.end = options.baseline;
        builder.metrics.line_count = codepoints.empty() ? 0 : 1;
        std::optional<std::uint32_t> previous;
        for (std::size_t i = 0; i < codepoints.size(); ++i) {
            const auto [codepoint, offset] = codepoints[i];
            if (codepoint == U'\r' || codepoint == U'\n') {
                if (codepoint == U'\r' && i + 1 < codepoints.size() && codepoints[i + 1].value == U'\n')
                    ++i;
                builder.metrics.width = std::max(builder.metrics.width, x - start_x);
                x = start_x;
                y += options.y_direction == ClipY::DOWN ? line_step : -line_step;
                ++builder.metrics.line_count;
                previous.reset();
                continue;
            }
            if (codepoint == U'\t') {
                const auto tab = atlas.glyph(U' ').advance * options.em_size * options.tab_spaces;
                if (!std::isfinite(tab) || tab <= 0)
                    throw std::invalid_argument("Tab stops require a positive space advance");
                const auto next = start_x + (std::floor((x - start_x) / tab) + 1) * tab;
                resource::MSDFGlyph glyph;
                glyph.advance = (next - x) / options.em_size;
                builder.glyphs.push_back({codepoint, offset, glyph, {x, y}, next - x});
                x = next;
                builder.metrics.width = std::max(builder.metrics.width, x - start_x);
                previous.reset();
                continue;
            }
            const auto glyph = atlas.glyph(codepoint);
            if (previous) {
                x += options.letter_spacing;
                if (options.kerning)
                    x += atlas.font().kerning(*previous, glyph.index) * options.em_size;
            }
            const auto advance = glyph.advance * options.em_size;
            builder.glyphs.push_back({codepoint, offset, glyph, {x, y}, advance});
            if (glyph.drawable())
                builder.append_quad(glyph, x, y, options);
            x += advance;
            builder.metrics.width = std::max(builder.metrics.width, x - start_x);
            previous = glyph.index;
        }
        builder.metrics.end = {x, y};
        builder.metrics.height = builder.metrics.line_count * line_step;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(builder.metrics.width) || !std::isfinite(builder.metrics.height))
            throw std::overflow_error("Text metrics cannot be represented");
        TextGeometry geometry(builder.points, builder.triangles);
        TextMesh mesh(std::move(geometry), builder.vertices, builder.faces);
        return TextLayout(atlas, std::move(mesh), std::move(builder.glyphs), std::move(builder.ranges), builder.metrics);
    }

    TextLayout layout_text(resource::MSDFAtlas& atlas, std::u8string_view utf8, const TextOptions& options) {
        return layout_text(atlas, std::string_view(reinterpret_cast<const char*>(utf8.data()), utf8.size()), options);
    }

    TextBuffers TextLayout::make_buffers() const {
        const auto points = _mesh.points();
        const auto triangles = _mesh.indices();
        const auto uv = _mesh.vertex_column<&VertexAttributes::uv>();
        const auto index_count = checked_product(triangles.size(), 3);
        TextBuffers result{resource::MSDFPixelBuffer(checked_product(points.size(), sizeof(TextVertex))),
                           resource::MSDFPixelBuffer(checked_product(index_count, sizeof(std::uint32_t))), points.size(), index_count};
        auto vertices = result.vertices.bytes();
        for (std::size_t i = 0; i < points.size(); ++i) {
            const TextVertex vertex{{points[i].value.___AN1, points[i].value.___AN2}, uv[i]};
            std::memcpy(vertices.data() + i * sizeof(TextVertex), &vertex, sizeof(vertex));
        }
        auto indices = result.indices.bytes();
        for (std::size_t i = 0; i < triangles.size(); ++i) {
            const auto& t = triangles[i];
            const std::array<std::uint32_t, 3> triangle{t.idx1.index, t.idx2.index, t.idx3.index};
            std::memcpy(indices.data() + i * sizeof(triangle), triangle.data(), sizeof(triangle));
        }
        return result;
    }
} // namespace AHO_NAMESPACE::x::text
