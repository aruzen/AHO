#include "support/render_fixture.hpp"
#include "support/read_image.hpp"
namespace {
    using RenderPassBeginVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(RenderPassBeginVulkanIntegrationTest, RetainsPassAndFramebufferAndClearsBlack) {
        VSL_NAMESPACE::command::RenderPassBegin begin(*pass, *framebuffer);
        EXPECT_EQ(begin.renderPass._data, pass->_data);
        EXPECT_EQ(begin.frameBuffer._data, framebuffer->_data);
        EXPECT_FALSE(begin.clear_color.has_value());
        EXPECT_EQ(render([](auto&) {}), (std::array<std::uint8_t, 4>{0, 0, 0, 0}));
    }
} // namespace

namespace {
    TEST_F(RenderPassBeginVulkanIntegrationTest, ChecksClearTypesCountsRenderAreaAndTargetIndex) {
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearUint{3}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{}, VSL_NAMESPACE::ClearColor{}})),
                     std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {}, {1, 0, 8, 8})), std::out_of_range);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {}, {}, 1)), std::out_of_range);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto pixels = vsl_test::read_image(manager, *color, VSL_NAMESPACE::ImageLayout::General);
        ASSERT_EQ(pixels.size(), 8u * 8u * 4u);
        EXPECT_EQ(pixels[0], std::byte{255});
        EXPECT_EQ(pixels[1], std::byte{0});
        EXPECT_EQ(pixels[3], std::byte{255});
    }
} // namespace

namespace {
    TEST_F(RenderPassBeginVulkanIntegrationTest, KeyedClearsValidateIdsTypesAndRetainDefaults) {
        auto begin = VSL_NAMESPACE::command::RenderPassBegin::withClears(*framebuffer, {});
        ASSERT_EQ(begin.clearValues.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<VSL_NAMESPACE::ClearColor>(begin.clearValues[0]));
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin::withClears(*framebuffer, {{0, VSL_NAMESPACE::ClearUint{1}}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin::withClears(*framebuffer, {{0, VSL_NAMESPACE::ClearColor{}}, {0, VSL_NAMESPACE::ClearColor{}}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin::withClears(*framebuffer, {{1, VSL_NAMESPACE::ClearColor{}}})), std::out_of_range);
        auto manager = make_manager();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin::withClears(*framebuffer, {{0, VSL_NAMESPACE::ClearColor{0, 1, 0, 1}}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto bytes = vsl_test::read_image(manager, *color, VSL_NAMESPACE::ImageLayout::General);
        EXPECT_EQ(bytes[0], std::byte{0});
        EXPECT_EQ(bytes[1], std::byte{255});
    }
} // namespace
