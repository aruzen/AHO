#pragma once
#include <AHO/x/resource/msdf_font.hpp>
#include <AHO/x/core/mesh.hpp>
#include <AHO/core/math/conventions.hpp>

namespace AHO_NAMESPACE::x::text {
    struct VertexAttributes {
        std::array<float, 2> uv{};
    };
    struct TriangleAttributes {
        std::uint32_t page = 0;
    };
    using TextGeometry = Polygon<float, VSL_NAMESPACE::D2, coordinate::DefaultCoordinateInfo<VSL_NAMESPACE::D2>::Value, std::uint32_t>;
    using TextVertexStorage = MeshAttributes<VertexAttributes, MeshAttributeMembers<&VertexAttributes::uv>>;
    using TextTriangleStorage = MeshAttributes<TriangleAttributes, MeshAttributeMembers<&TriangleAttributes::page>>;
    using TextMesh = Mesh<TextGeometry, VertexAttributes, TriangleAttributes, TextVertexStorage, TextTriangleStorage>;
    // 転送用AoS。Meshの列から別々に転送する場合はこの形式に依存しなくてよい。
    struct TextVertex {
        std::array<float, 2> position{}, uv{};
    };
    struct TextBuffers {
        resource::MSDFPixelBuffer vertices, indices;
        std::size_t vertex_count = 0, index_count = 0;
    };
    struct TextOptions {
        d2::PointD baseline{0, 0};
        double em_size = 32, line_spacing = 1, letter_spacing = 0;
        std::uint32_t tab_spaces = 4;
        bool kerning = true;
        ClipY y_direction = ClipY::DOWN;
    };
    struct PositionedGlyph {
        char32_t codepoint = 0;
        std::size_t byte_offset = 0;
        resource::MSDFGlyph glyph;
        d2::PointD baseline{0, 0};
        double advance = 0;
    };
    // 文字順を保った連続範囲。pageごとに並べ替えると重なり順が変わるため行わない。
    struct TextDrawRange {
        std::uint32_t page = 0;
        std::size_t first_index = 0, index_count = 0;
    };
    struct TextMetrics {
        d2::PointD minimum{0, 0}, maximum{0, 0}, end{0, 0};
        double width = 0, height = 0;
        std::size_t line_count = 0;
        bool has_ink = false;
    };
    class TextLayout;
    TextLayout layout_text(resource::MSDFAtlas& atlas, std::string_view utf8, const TextOptions& options = {});
    TextLayout layout_text(resource::MSDFAtlas& atlas, std::u8string_view utf8, const TextOptions& options = {});
    class TextLayout {
        resource::MSDFAtlas _atlas;
        TextMesh _mesh;
        std::vector<PositionedGlyph> _glyphs;
        std::vector<TextDrawRange> _ranges;
        TextMetrics _metrics;
        TextLayout(resource::MSDFAtlas atlas, TextMesh mesh, std::vector<PositionedGlyph> glyphs,
                   std::vector<TextDrawRange> ranges, TextMetrics metrics);
        friend TextLayout layout_text(resource::MSDFAtlas&, std::string_view, const TextOptions&);

    public:
        const resource::MSDFAtlas& atlas() const;
        const TextMesh& mesh() const;
        std::span<const PositionedGlyph> glyphs() const;
        std::span<const TextDrawRange> ranges() const;
        const TextMetrics& metrics() const;
        // レイアウト変更時に一度作る。indicesはuint32、verticesはTextVertexの並び。
        TextBuffers make_buffers() const;
    };
} // namespace AHO_NAMESPACE::x::text
