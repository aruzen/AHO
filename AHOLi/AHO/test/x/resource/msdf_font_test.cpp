#include <AHO/x/resource/msdf_font.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <limits>

namespace {
    namespace r = aho::x::resource;
    r::FontFace test_font();
    unsigned median_pixel(const r::MSDFAtlasPage& page, std::uint32_t x, std::uint32_t y);

    r::FontFace test_font() {
        return r::FontFace(std::filesystem::path(AHO_MSDF_TEST_FONT));
    }

    unsigned median_pixel(const r::MSDFAtlasPage& page, std::uint32_t x, std::uint32_t y) {
        const auto offset = (std::size_t(y) * page.width + x) * 4;
        const auto bytes = page.pixels.bytes();
        std::array<unsigned, 3> channels{std::to_integer<unsigned>(bytes[offset]), std::to_integer<unsigned>(bytes[offset + 1]), std::to_integer<unsigned>(bytes[offset + 2])};
        std::sort(channels.begin(), channels.end());
        return channels[1];
    }

    TEST(MSDFFontTest, MetricsAndKerningUseNormalizedEmUnits) {
        const auto font = test_font();
        const auto metrics = font.metrics();
        EXPECT_DOUBLE_EQ(metrics.ascent, .8);
        EXPECT_DOUBLE_EQ(metrics.descent, -.2);
        EXPECT_DOUBLE_EQ(metrics.line_height, 1.2);
        ASSERT_TRUE(font.glyph_index(U'A'));
        ASSERT_TRUE(font.glyph_index(U'V'));
        EXPECT_DOUBLE_EQ(font.kerning(*font.glyph_index(U'A'), *font.glyph_index(U'V')), -.08);
        EXPECT_FALSE(font.glyph_index(U'\U0001f98a'));
        EXPECT_THROW(font.glyph_index(char32_t{0xd800}), std::invalid_argument);
        EXPECT_THROW(font.glyph_index(char32_t{0x110000}), std::invalid_argument);
    }

    TEST(MSDFFontTest, FontBytesAreOwnedAndInvalidFontsAreRejected) {
        std::ifstream file(AHO_MSDF_TEST_FONT, std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        r::FontFace font(std::as_bytes(std::span(bytes)));
        std::fill(bytes.begin(), bytes.end(), 0);
        r::MSDFAtlas atlas(font);
        EXPECT_TRUE(atlas.glyph(U'A').drawable());
        EXPECT_THROW((r::FontFace(std::span<const std::byte>{})), std::invalid_argument);
        const std::array invalid{std::byte{1}, std::byte{2}, std::byte{3}};
        EXPECT_THROW((r::FontFace(invalid)), std::invalid_argument);
        EXPECT_THROW((r::FontFace(std::filesystem::path(AHO_MSDF_TEST_FONT) / "missing.ttf")), std::runtime_error);
    }

    TEST(MSDFAtlasTest, GlyphBitmapHasPositiveInteriorNegativeExteriorAndIndependentChannels) {
        r::MSDFAtlas atlas(test_font());
        const auto glyph = atlas.glyph(U'A');
        ASSERT_TRUE(glyph.drawable());
        EXPECT_DOUBLE_EQ(glyph.advance, .6);
        EXPECT_DOUBLE_EQ(glyph.ink_bounds.left, .05);
        EXPECT_DOUBLE_EQ(glyph.ink_bounds.top, .7);
        const auto& page = atlas.page(glyph.page);
        const auto& rect = glyph.rectangle;
        EXPECT_EQ(r::MSDFAtlasPage::FORMAT, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        const auto x = rect.x + std::uint32_t((.3 - glyph.plane_bounds.left) * atlas.settings().pixels_per_em);
        const auto y = rect.y + std::uint32_t((glyph.plane_bounds.top - .1) * atlas.settings().pixels_per_em);
        EXPECT_GT(median_pixel(page, x, y), 128u);
        EXPECT_LT(median_pixel(page, rect.x, rect.y), 128u);
        const auto update = atlas.updates_since(0).front();
        bool multichannel = false;
        const auto pixels = update.pixels.bytes();
        for (std::size_t i = 0; i < pixels.size(); i += 4)
            multichannel |= pixels[i] != pixels[i + 1] || pixels[i + 1] != pixels[i + 2];
        EXPECT_TRUE(multichannel);
        EXPECT_GT(page.unit_range()[0], 0);
    }

    TEST(MSDFAtlasTest, SpacesAndCachedGlyphsDoNotAllocateOrRegenerate) {
        r::MSDFAtlas atlas(test_font());
        EXPECT_FALSE(atlas.glyph(U' ').drawable());
        EXPECT_EQ(atlas.page_count(), 0u);
        EXPECT_DOUBLE_EQ(atlas.glyph(U' ').advance, .3);
        atlas.prepare("AVA");
        const auto revision = atlas.revision();
        const auto glyphs = atlas.glyph_count();
        atlas.prepare(u8"AVA\r\n\t");
        EXPECT_EQ(atlas.revision(), revision);
        EXPECT_EQ(atlas.glyph_count(), glyphs);
        EXPECT_TRUE(atlas.updates_since(revision).empty());
    }

    TEST(MSDFAtlasTest, FullPagesKeepExistingCoordinatesPixelsAndUpdatesAlive) {
        r::MSDFAtlas atlas(test_font(), {.pixels_per_em = 24, .pixel_range = 4, .page_width = 32, .page_height = 32, .max_pages = 2});
        const auto a = atlas.glyph(U'A');
        const auto* page = &atlas.page(0);
        const auto updates = atlas.updates_since(0);
        const auto before = std::vector<std::byte>(updates[0].pixels.bytes().begin(), updates[0].pixels.bytes().end());
        const auto v = atlas.glyph(U'V');
        EXPECT_EQ(a.page, 0u);
        EXPECT_EQ(v.page, 1u);
        EXPECT_EQ(&atlas.page(0), page);
        const auto again = atlas.glyph(U'A');
        EXPECT_FLOAT_EQ(again.uv.left, a.uv.left);
        EXPECT_FLOAT_EQ(again.uv.top, a.uv.top);
        EXPECT_THROW(atlas.glyph(U'日'), std::length_error);
        EXPECT_EQ(atlas.revision(), 2u);
        EXPECT_EQ(atlas.glyph_count(), 2u);
        for (std::uint32_t row = 0; row < a.rectangle.height; ++row) {
            const auto pixels = page->pixels.bytes().subspan((std::size_t(a.rectangle.y + row) * page->width + a.rectangle.x) * 4, a.rectangle.width * 4);
            EXPECT_TRUE(std::equal(pixels.begin(), pixels.end(), before.begin() + std::size_t(row) * a.rectangle.width * 4));
        }
        EXPECT_EQ(atlas.updates_since(1).size(), 1u);
        EXPECT_EQ(updates[0].pixels.size(), before.size());
    }

    TEST(MSDFAtlasTest, MissingGlyphReplacementSharesTheRealGlyphCache) {
        r::MSDFAtlas atlas(test_font());
        const auto missing = atlas.glyph(U'\U0001f98a');
        const auto replacement = atlas.glyph(U'\ufffd');
        EXPECT_TRUE(missing.fallback);
        EXPECT_FALSE(replacement.fallback);
        EXPECT_EQ(missing.index, replacement.index);
        EXPECT_EQ(atlas.glyph_count(), 1u);
        r::MSDFAtlas strict(test_font(), {.missing_glyph = r::MissingGlyphPolicy::ERROR_});
        EXPECT_THROW(strict.glyph(U'\U0001f98a'), std::invalid_argument);
        EXPECT_EQ(strict.glyph_count(), 0u);
    }

    TEST(MSDFAtlasTest, UpdateBuffersOutliveAtlasAndOversizedGlyphDoesNotChangeCache) {
        const auto updates = [] {
            r::MSDFAtlas atlas(test_font());
            atlas.prepare("A");
            return atlas.updates_since(0);
        }();
        ASSERT_EQ(updates.size(), 1u);
        EXPECT_EQ(updates[0].pixels.size(), std::size_t(updates[0].rectangle.width) * updates[0].rectangle.height * 4);
        EXPECT_TRUE(std::any_of(updates[0].pixels.bytes().begin(), updates[0].pixels.bytes().end(), [](auto byte) { return byte != std::byte{}; }));
        r::MSDFAtlas small(test_font(), {.page_width = 16, .page_height = 16});
        EXPECT_THROW(small.glyph(U'A'), std::length_error);
        EXPECT_EQ(small.glyph_count(), 0u);
        EXPECT_EQ(small.page_count(), 0u);
        EXPECT_EQ(small.revision(), 0u);
        EXPECT_FALSE(small.glyph(U' ').drawable());
    }

    TEST(MSDFAtlasTest, InvalidUtf8AndSettingsFailBeforeGeneratingGlyphs) {
        r::MSDFAtlas atlas(test_font());
        for (const auto text : {std::string("A\xc0\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xe3\x81"), std::string("\x80")})
            EXPECT_THROW(atlas.prepare(text), std::invalid_argument);
        EXPECT_EQ(atlas.glyph_count(), 0u);
        EXPECT_THROW((r::MSDFAtlas(test_font(), {.pixels_per_em = 0})), std::invalid_argument);
        EXPECT_THROW((r::MSDFAtlas(test_font(), {.pixel_range = std::numeric_limits<double>::infinity()})), std::invalid_argument);
        EXPECT_THROW((r::MSDFAtlas(test_font(), {.page_width = 0})), std::invalid_argument);
        EXPECT_THROW((r::MSDFAtlas(test_font(), {.max_pages = 0})), std::invalid_argument);
    }
} // namespace
