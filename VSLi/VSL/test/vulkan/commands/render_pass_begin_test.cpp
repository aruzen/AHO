#include "support/render_fixture.hpp"
#include "support/read_image.hpp"
namespace {
    using RenderPassBeginVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(RenderPassBeginVulkanIntegrationTest, RetainsPassAndFramebufferAndClearsBlack) {
        vsl::command::RenderPassBegin begin(*pass, *framebuffer);
        EXPECT_EQ(begin.renderPass._data, pass->_data);
        EXPECT_EQ(begin.frameBuffer._data, framebuffer->_data);
        EXPECT_FALSE(begin.clear_color.has_value());
        EXPECT_EQ(render([](auto &) {}), (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
    }
}

namespace {
    TEST_F(RenderPassBeginVulkanIntegrationTest, ChecksClearTypesCountsRenderAreaAndTargetIndex) {
        EXPECT_THROW((vsl::command::RenderPassBegin(*framebuffer, {vsl::ClearUint{3}})), std::invalid_argument);
        EXPECT_THROW((vsl::command::RenderPassBegin(*framebuffer, {vsl::ClearColor{}, vsl::ClearColor{}})),
                     std::invalid_argument);
        EXPECT_THROW((vsl::command::RenderPassBegin(*framebuffer, {}, {1, 0, 8, 8})), std::out_of_range);
        EXPECT_THROW((vsl::command::RenderPassBegin(*framebuffer, {}, {}, 1)), std::out_of_range);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer, {vsl::ClearColor{1, 0, 0, 1}})
                  << vsl::command::RenderPassEnd{};
        }
        auto pixels = vsl_test::read_image(manager, *color, vsl::ImageLayout::General);
        ASSERT_EQ(pixels.size(), 8u * 8u * 4u);
        EXPECT_EQ(pixels[0], std::byte{255});
        EXPECT_EQ(pixels[1], std::byte{0});
        EXPECT_EQ(pixels[3], std::byte{255});
    }
}
