#include <AHO/x/text/text_layout.hpp>
#include <gtest/gtest.h>
#include <cstring>
#include <limits>

namespace {
    namespace r = aho::x::resource;
    namespace t = aho::x::text;
    r::MSDFAtlas test_atlas();
    r::MSDFAtlas test_atlas() {
        return r::MSDFAtlas(r::FontFace(std::filesystem::path(AHO_MSDF_TEST_FONT)));
    }

    TEST(TextLayoutTest, JapaneseUtf8OffsetsMeshAndInkBoundsUseFontMetrics) {
        auto atlas = test_atlas();
        const auto text = t::layout_text(atlas, u8"A日本語", {.baseline = {10, 100}, .em_size = 100});
        ASSERT_EQ(text.glyphs().size(), 4u);
        EXPECT_EQ(text.glyphs()[1].codepoint, U'日');
        EXPECT_EQ(text.glyphs()[1].byte_offset, 1u);
        EXPECT_EQ(text.glyphs()[2].byte_offset, 4u);
        EXPECT_EQ(text.glyphs()[3].byte_offset, 7u);
        EXPECT_DOUBLE_EQ(text.glyphs()[1].baseline.value.___AN1, 70);
        EXPECT_DOUBLE_EQ(text.metrics().width, 300);
        EXPECT_DOUBLE_EQ(text.metrics().height, 120);
        EXPECT_TRUE(text.metrics().has_ink);
        EXPECT_EQ(text.mesh().vertex_count(), 16u);
        EXPECT_EQ(text.mesh().size(), 8u);
        EXPECT_DOUBLE_EQ(text.metrics().minimum.value.___AN1, 15);
        EXPECT_DOUBLE_EQ(text.metrics().minimum.value.___AN2, 30);
        EXPECT_DOUBLE_EQ(text.metrics().maximum.value.___AN2, 110);
    }

    TEST(TextLayoutTest, KerningTrackingWhitespaceTabsAndLineBreaksRemainDistinct) {
        auto atlas = test_atlas();
        const auto kerned = t::layout_text(atlas, "AV", {.em_size = 100});
        EXPECT_DOUBLE_EQ(kerned.glyphs()[1].baseline.value.___AN1, 52);
        EXPECT_DOUBLE_EQ(kerned.metrics().width, 112);
        const auto tracked = t::layout_text(atlas, "AV", {.em_size = 100, .letter_spacing = 2, .kerning = false});
        EXPECT_DOUBLE_EQ(tracked.glyphs()[1].baseline.value.___AN1, 62);
        EXPECT_DOUBLE_EQ(tracked.metrics().width, 122);
        const auto lines = t::layout_text(atlas, "A\tV\r\n A\rV\n", {.em_size = 100});
        EXPECT_EQ(lines.metrics().line_count, 4u);
        EXPECT_DOUBLE_EQ(lines.glyphs()[2].baseline.value.___AN1, 120);
        EXPECT_DOUBLE_EQ(lines.glyphs()[3].baseline.value.___AN2, 120);
        EXPECT_DOUBLE_EQ(lines.metrics().end.value.___AN2, 360);
        EXPECT_EQ(lines.mesh().vertex_count(), 16u);
        const auto spaces = t::layout_text(atlas, "  ", {.em_size = 100});
        EXPECT_DOUBLE_EQ(spaces.metrics().width, 60);
        EXPECT_FALSE(spaces.metrics().has_ink);
        EXPECT_TRUE(spaces.mesh().empty());
    }

    TEST(TextLayoutTest, AxisPolicyMirrorsPositionsAndPreservesPositiveTriangleWinding) {
        auto atlas = test_atlas();
        const auto down = t::layout_text(atlas, "A\nV", {.baseline = {10, 100}, .em_size = 40});
        const auto up = t::layout_text(atlas, "A\nV", {.baseline = {10, 100}, .em_size = 40, .y_direction = aho::ClipY::UP});
        const auto a = down.mesh().points(), b = up.mesh().points();
        for (std::size_t i = 0; i < a.size(); ++i) {
            EXPECT_FLOAT_EQ(a[i].value.___AN1, b[i].value.___AN1);
            EXPECT_NEAR(a[i].value.___AN2 + b[i].value.___AN2, 200, 2e-5);
        }
        for (const auto* mesh : {&up.mesh(), &down.mesh()})
            for (const auto triangle : mesh->triangles()) {
                const auto ab = triangle.pos2 - triangle.pos1, ac = triangle.pos3 - triangle.pos1;
                EXPECT_GT(ab.cross(ac).value.___AN1, 0);
            }
    }

    TEST(TextLayoutTest, PageRangesPreserveTextOrderAndPackedBuffersMatchMesh) {
        r::MSDFAtlas atlas(r::FontFace(std::filesystem::path(AHO_MSDF_TEST_FONT)),
                           {.pixels_per_em = 24, .pixel_range = 4, .page_width = 32, .page_height = 32});
        const auto text = t::layout_text(atlas, "AVA");
        ASSERT_EQ(text.ranges().size(), 3u);
        for (std::size_t i = 0; i < 3; ++i) {
            EXPECT_EQ(text.ranges()[i].page, i == 1 ? 1u : 0u);
            EXPECT_EQ(text.ranges()[i].first_index, i * 6);
            EXPECT_EQ(text.ranges()[i].index_count, 6u);
        }
        const auto buffers = text.make_buffers();
        EXPECT_EQ(buffers.vertex_count, 12u);
        EXPECT_EQ(buffers.index_count, 18u);
        EXPECT_EQ(buffers.vertices.size(), 12 * sizeof(t::TextVertex));
        for (std::size_t i = 0; i < buffers.vertex_count; ++i) {
            t::TextVertex vertex;
            std::memcpy(&vertex, buffers.vertices.bytes().data() + i * sizeof(vertex), sizeof(vertex));
            EXPECT_FLOAT_EQ(vertex.position[0], text.mesh().points()[i].value.___AN1);
            EXPECT_FLOAT_EQ(vertex.position[1], text.mesh().points()[i].value.___AN2);
            EXPECT_EQ(vertex.uv, text.mesh().vertex_column<&t::VertexAttributes::uv>()[i]);
        }
        std::array<std::uint32_t, 6> first;
        std::memcpy(first.data(), buffers.indices.data(), sizeof(first));
        EXPECT_EQ(first, (std::array<std::uint32_t, 6>{0, 1, 2, 2, 3, 0}));
    }

    TEST(TextLayoutTest, LayoutKeepsAtlasAliveAndRepeatedLayoutsDoNotGenerateAgain) {
        const auto text = [] { auto atlas = test_atlas(); return t::layout_text(atlas, "AV"); }();
        EXPECT_TRUE(text.atlas().page(0).pixels.size());
        auto atlas = text.atlas();
        const auto revision = atlas.revision();
        const auto another = t::layout_text(atlas, "AV");
        EXPECT_EQ(atlas.revision(), revision);
        EXPECT_TRUE(atlas.updates_since(revision).empty());
        EXPECT_EQ(another.mesh().vertex_count(), text.mesh().vertex_count());
    }

    TEST(TextLayoutTest, EmptyTextInvalidOptionsAndInvalidUtf8AreHandledBeforeGeneration) {
        auto atlas = test_atlas();
        const auto empty = t::layout_text(atlas, "");
        EXPECT_TRUE(empty.mesh().empty());
        EXPECT_EQ(empty.metrics().line_count, 0u);
        EXPECT_EQ(empty.make_buffers().vertices.size(), 0u);
        EXPECT_THROW(t::layout_text(atlas, "A", {.em_size = 0}), std::invalid_argument);
        EXPECT_THROW(t::layout_text(atlas, "A", {.line_spacing = -1}), std::invalid_argument);
        EXPECT_THROW(t::layout_text(atlas, "A", {.tab_spaces = 0}), std::invalid_argument);
        EXPECT_THROW(t::layout_text(atlas, "A\x80"), std::invalid_argument);
        EXPECT_EQ(atlas.glyph_count(), 0u);
        EXPECT_THROW(t::layout_text(atlas, "A", {.em_size = std::numeric_limits<double>::max()}), std::overflow_error);
    }
} // namespace
