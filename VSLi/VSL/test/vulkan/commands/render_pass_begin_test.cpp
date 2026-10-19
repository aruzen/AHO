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

namespace {
    class LegacyClearVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        void SetUp() override {
            require_validation = true;
            VulkanIntegrationTest::SetUp();
        }
    };
    class LegacyClearFormatVulkanIntegrationTest : public LegacyClearVulkanIntegrationTest,
                                                  public testing::WithParamInterface<vk::Format> {};
    vsl::FrameBufferAccessor legacy_metadata_target(vsl::RenderPassAccessor pass) {
        auto target = std::make_shared<vsl::_impl::FrameBuffer_impl>();
        target->renderPass = std::move(pass._data);
        target->swapChainFramebuffers.emplace_back();
        target->extent = vk::Extent2D{2, 2};
        return {std::move(target)};
    }
    TEST_P(LegacyClearFormatVulkanIntegrationTest, ExplicitRgbMustNotBypassIntegerOrDepthClearValidation) {
        const auto format = GetParam();
        const bool depth = format == vk::Format::eD32Sfloat;
        vsl::RenderPassDescription description;
        vsl::AttachmentDescription attachment{.format = {static_cast<vsl::data_format::___FormatTypes>(format), 4},
                                               .finalLayout = depth ? vsl::ImageLayout::DepthStencilAttachmentOptimal : vsl::ImageLayout::ColorAttachmentOptimal};
        if (depth)
            description.setDepthStencil(attachment);
        else
            description.addColor(attachment);
        vsl::RenderPass pass(device, description);
        auto target = legacy_metadata_target(pass);
        const vsl::command::RenderPassBegin defaults(pass, target);
        ASSERT_EQ(defaults.clearValues.size(), 1u);
        const auto kind = defaults.clearValues[0].index();
        EXPECT_NE(kind, 0u);
        EXPECT_FALSE(defaults.clear_color);
        EXPECT_FALSE(defaults.targetIndex);
        float red = 0.25f, green = 0.5f, blue = 0.75f;
        const vsl::__VSLDRGBColorAccessor<float> color{red, green, blue};
        EXPECT_THROW((vsl::command::RenderPassBegin(pass, target, color)), std::invalid_argument);
        EXPECT_EQ(vsl::command::RenderPassBegin(pass, target).clearValues[0].index(), kind);
    }
    INSTANTIATE_TEST_SUITE_P(NumericFormats, LegacyClearFormatVulkanIntegrationTest,
                            testing::Values(vk::Format::eR32Sint, vk::Format::eR32Uint, vk::Format::eD32Sfloat),
                            [](const testing::TestParamInfo<vk::Format>& info) { return vk::to_string(info.param); });
    TEST_F(LegacyClearVulkanIntegrationTest, ExplicitRgbIsRejectedWhenThereAreNoAttachments) {
        vsl::RenderPass pass(device, vsl::RenderPassDescription{});
        auto target = legacy_metadata_target(pass);
        EXPECT_TRUE(vsl::command::RenderPassBegin(pass, target).clearValues.empty());
        float red = 1, green = 0, blue = 0;
        const vsl::__VSLDRGBColorAccessor<float> color{red, green, blue};
        EXPECT_THROW((vsl::command::RenderPassBegin(pass, target, color)), std::invalid_argument);
    }
    TEST_F(LegacyClearVulkanIntegrationTest, PickingOverloadAlsoRejectsIntegerRgbClearsAndPreservesTypedDefaults) {
        for (const auto format : {vk::Format::eR32Sint, vk::Format::eR32Uint}) {
            SCOPED_TRACE(vk::to_string(format));
            vsl::SwapchainAccessor chain;
            chain._data = std::make_shared<vsl::_impl::Swapchain_impl>();
            chain._data->device = device._data;
            chain._data->extent = chain._data->swapChainExtent = vk::Extent2D{2, 2};
            chain._data->swapChainImageFormat = format;
            vsl::IDPickingRenderPass pass(chain);
            auto target = legacy_metadata_target(pass);
            const vsl::command::IDPickingRenderPassBegin defaults(pass, target);
            ASSERT_EQ(defaults.clearValues.size(), 2u);
            EXPECT_EQ(defaults.clearValues[0].index(), format == vk::Format::eR32Sint ? 1u : 2u);
            EXPECT_TRUE(std::holds_alternative<vsl::ClearUint>(defaults.clearValues[1]));
            float red = 1, green = 0, blue = 0;
            const vsl::__VSLDRGBColorAccessor<float> color{red, green, blue};
            EXPECT_THROW((vsl::command::IDPickingRenderPassBegin(pass, target, color)), std::invalid_argument);
        }
    }
    TEST_F(LegacyClearVulkanIntegrationTest, PickingRgbPreservesTheIdAttachmentDefaultAndZeroAlpha) {
        vsl::SwapchainAccessor chain;
        chain._data = std::make_shared<vsl::_impl::Swapchain_impl>();
        chain._data->device = device._data;
        chain._data->extent = chain._data->swapChainExtent = vk::Extent2D{2, 2};
        chain._data->swapChainImageFormat = vk::Format::eR8G8B8A8Unorm;
        vsl::IDPickingRenderPass pass(chain);
        auto target = legacy_metadata_target(pass);
        float red = 0.25f, green = 0.5f, blue = 0.75f;
        const vsl::__VSLDRGBColorAccessor<float> color{red, green, blue};
        const vsl::command::IDPickingRenderPassBegin begin(pass, target, color);
        ASSERT_EQ(begin.clearValues.size(), 2u);
        ASSERT_TRUE(std::holds_alternative<vsl::ClearColor>(begin.clearValues[0]));
        const auto clear = std::get<vsl::ClearColor>(begin.clearValues[0]);
        EXPECT_EQ(clear.r, red);
        EXPECT_EQ(clear.g, green);
        EXPECT_EQ(clear.b, blue);
        EXPECT_EQ(clear.a, 0.f);
        ASSERT_TRUE(std::holds_alternative<vsl::ClearUint>(begin.clearValues[1]));
        const auto id = std::get<vsl::ClearUint>(begin.clearValues[1]);
        EXPECT_EQ(id.r, 0u);
        EXPECT_EQ(id.g, 0u);
        EXPECT_EQ(id.b, 0u);
        EXPECT_EQ(id.a, 0u);
        ASSERT_TRUE(begin.clear_color);
        EXPECT_EQ(begin.clear_color->r, red);
        EXPECT_FALSE(begin.targetIndex);
    }
    TEST_F(LegacyClearVulkanIntegrationTest, ClearColorForAnUnclearedAttachmentKeepsTheCommonConstructorsIgnoredValueSemantics) {
        vsl::RenderPassDescription description;
        description.addColor({.format = vsl::data_format::UnsignedInt, .load = vsl::LoadOp::DontCare});
        vsl::RenderPass pass(device, description);
        auto target = legacy_metadata_target(pass);
        float red = 1, green = 0, blue = 0;
        const vsl::__VSLDRGBColorAccessor<float> color{red, green, blue};
        EXPECT_NO_THROW((vsl::command::RenderPassBegin(target, {vsl::ClearColor{red, green, blue, 0}})));
        EXPECT_NO_THROW((vsl::command::RenderPassBegin(pass, target, color)));
    }
    TEST_F(RenderPassBeginVulkanIntegrationTest, LegacyRgbClearPreservesZeroAlphaAndReachesEveryPixel) {
        float red = 0.25f, green = 0.5f, blue = 0.75f;
        const vsl::__VSLDRGBColorAccessor<float> colorValue{red, green, blue};
        vsl::command::RenderPassBegin begin(*pass, *framebuffer, colorValue);
        ASSERT_TRUE(begin.clear_color);
        EXPECT_EQ(begin.clear_color->r, red);
        EXPECT_FALSE(begin.targetIndex);
        ASSERT_EQ(begin.clearValues.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<vsl::ClearColor>(begin.clearValues[0]));
        EXPECT_EQ(std::get<vsl::ClearColor>(begin.clearValues[0]).a, 0.f);
        auto manager = make_manager();
        {
            vsl::ComputePhase phase(manager);
            phase << begin << vsl::command::RenderPassEnd{};
        }
        const auto bytes = vsl_test::read_image(manager, *color, vsl::ImageLayout::General);
        ASSERT_EQ(bytes.size(), width * height * 4u);
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
            // UNORM8の丸めを1段階だけ許容し、alphaの既存値0は厳密一致で確認する。
            EXPECT_NEAR(std::to_integer<int>(bytes[offset]), 64, 1);
            EXPECT_NEAR(std::to_integer<int>(bytes[offset + 1]), 128, 1);
            EXPECT_NEAR(std::to_integer<int>(bytes[offset + 2]), 191, 1);
            EXPECT_EQ(bytes[offset + 3], std::byte{0});
        }
    }
} // namespace
