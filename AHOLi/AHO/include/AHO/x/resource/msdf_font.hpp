#pragma once
#include <AHO/resource/buffer.hpp>
#include <array>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace AHO_NAMESPACE::x::resource {
    struct FontMetrics {
        double ascent = 0, descent = 0, line_height = 0;
        double underline_position = 0, underline_thickness = 0;
    };
    // フォントのem単位、Y軸は上向き。
    struct GlyphBounds {
        double left = 0, bottom = 0, right = 0, top = 0;
    };
    struct AtlasRect {
        std::uint32_t x = 0, y = 0, width = 0, height = 0;
    };
    struct GlyphUV {
        float left = 0, top = 0, right = 0, bottom = 0;
    };
    inline constexpr std::uint32_t NO_ATLAS_PAGE = UINT32_MAX;
    struct MSDFGlyph {
        std::uint32_t index = 0, page = NO_ATLAS_PAGE;
        double advance = 0;
        GlyphBounds ink_bounds, plane_bounds;
        AtlasRect rectangle;
        GlyphUV uv;
        bool fallback = false;
        bool drawable() const { return page != NO_ATLAS_PAGE; }
    };
    enum class MissingGlyphPolicy {
        REPLACE,
        ERROR_
    };
    struct MSDFSettings {
        double pixels_per_em = 48, pixel_range = 6;
        std::uint32_t page_width = 512, page_height = 512, max_pages = 8;
        MissingGlyphPolicy missing_glyph = MissingGlyphPolicy::REPLACE;
    };
    using MSDFPixelBuffer = AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GENERAL>;
    struct MSDFAtlasPage {
        static constexpr auto FORMAT = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA;
        std::uint32_t width = 0, height = 0;
        double pixel_range = 0;
        MSDFPixelBuffer pixels;
        std::uint64_t revision = 0;
        std::array<float, 2> unit_range() const;
    };
    // pixelsは矩形内だけを隙間なく格納する。取得後に字形を追加しても保持できる。
    struct MSDFAtlasUpdate {
        std::uint64_t revision = 0;
        std::uint32_t page = 0;
        AtlasRect rectangle;
        MSDFPixelBuffer pixels;
    };
    class MSDFAtlas;
    class FontFace {
        struct Data;
        std::shared_ptr<Data> _data;
        Data& data() const;
        friend class MSDFAtlas;

    public:
        explicit FontFace(const std::filesystem::path& path);
        explicit FontFace(std::span<const std::byte> bytes);
        FontMetrics metrics() const;
        std::optional<std::uint32_t> glyph_index(char32_t codepoint) const;
        double kerning(std::uint32_t left, std::uint32_t right) const;
    };
    // コピーは字形キャッシュを共有する。追加は直列に行い、既存字形の配置は変えない。
    class MSDFAtlas {
        struct Data;
        std::shared_ptr<Data> _data;
        Data& data() const;

    public:
        explicit MSDFAtlas(FontFace font, MSDFSettings settings = {});
        const FontFace& font() const;
        const MSDFSettings& settings() const;
        MSDFGlyph glyph(char32_t codepoint);
        void prepare(std::string_view utf8);
        void prepare(std::u8string_view utf8);
        std::size_t glyph_count() const;
        std::size_t page_count() const;
        const MSDFAtlasPage& page(std::size_t index) const;
        std::uint64_t revision() const;
        std::vector<MSDFAtlasUpdate> updates_since(std::uint64_t revision) const;
    };
} // namespace AHO_NAMESPACE::x::resource
