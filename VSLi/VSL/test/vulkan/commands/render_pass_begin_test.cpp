#include "support/render_fixture.hpp"
#include "support/read_image.hpp"
namespace {
    using RenderPassBeginVulkanIntegrationTest = vsl_test::RenderVulkanIntegrationTest;
    TEST_F(RenderPassBeginVulkanIntegrationTest, RetainsPassAndFramebufferAndClearsBlack) {
        VSL_NAMESPACE::command::RenderPassBegin begin(*pass, *framebuffer);
        EXPECT_EQ(begin.render_pass.data_, pass->data_);
        EXPECT_EQ(begin.frame_buffer.data_, framebuffer->data_);
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
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto pixels = vsl_test::read_image(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
        ASSERT_EQ(pixels.size(), 8u * 8u * 4u);
        EXPECT_EQ(pixels[0], std::byte{255});
        EXPECT_EQ(pixels[1], std::byte{0});
        EXPECT_EQ(pixels[3], std::byte{255});
    }
} // namespace

namespace {
    TEST_F(RenderPassBeginVulkanIntegrationTest, KeyedClearsValidateIdsTypesAndRetainDefaults) {
        auto begin = VSL_NAMESPACE::command::RenderPassBegin::WithClears(*framebuffer, {});
        ASSERT_EQ(begin.clear_values.size(), 1u);
        EXPECT_TRUE(std::holds_alternative<VSL_NAMESPACE::ClearColor>(begin.clear_values[0]));
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin::WithClears(*framebuffer, {{0, VSL_NAMESPACE::ClearUint{1}}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin::WithClears(*framebuffer, {{0, VSL_NAMESPACE::ClearColor{}}, {0, VSL_NAMESPACE::ClearColor{}}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin::WithClears(*framebuffer, {{1, VSL_NAMESPACE::ClearColor{}}})), std::out_of_range);
        auto manager = make_manager();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin::WithClears(*framebuffer, {{0, VSL_NAMESPACE::ClearColor{0, 1, 0, 1}}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto bytes = vsl_test::read_image(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
        EXPECT_EQ(bytes[0], std::byte{0});
        EXPECT_EQ(bytes[1], std::byte{255});
    }
} // namespace

namespace {
    struct PackedIntegerClearCase {
        vk::Format format;
        bool is_signed;
    };
    void PrintTo(const PackedIntegerClearCase& value, std::ostream* stream) {
        *stream << vk::to_string(value.format);
    }
    class PackedIntegerClearTest : public testing::TestWithParam<PackedIntegerClearCase> {};
    VSL_NAMESPACE::FrameBufferAccessor metadata_framebuffer(vk::Format format) {
        auto description = std::make_shared<VSL_NAMESPACE::RenderPassDescription>();
        description->add_color({.format = {static_cast<VSL_NAMESPACE::data_format::FormatTypes>(format), 4}});
        auto pass = std::make_shared<VSL_NAMESPACE::impl::RenderPassImpl>();
        pass->description = std::move(description);
        auto target = std::make_shared<VSL_NAMESPACE::impl::FrameBufferImpl>();
        target->render_pass = std::move(pass);
        target->swap_chain_framebuffers.emplace_back();
        target->extent = vk::Extent2D{1, 1};
        return {std::move(target)};
    }
    TEST_P(PackedIntegerClearTest, DefaultsAndExplicitClearsUseTheIntegerNumericType) {
        const auto [format, is_signed] = GetParam();
        auto target = metadata_framebuffer(format);
        const VSL_NAMESPACE::command::RenderPassBegin defaults(target);
        ASSERT_EQ(defaults.clear_values.size(), 1u);
        EXPECT_EQ(std::holds_alternative<VSL_NAMESPACE::ClearInt>(defaults.clear_values[0]), is_signed);
        EXPECT_EQ(std::holds_alternative<VSL_NAMESPACE::ClearUint>(defaults.clear_values[0]), !is_signed);
        const VSL_NAMESPACE::ClearValue correct = is_signed ? VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearInt{-7, 15, -1, 1}}
                                                            : VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearUint{17, 31, 47, 2}};
        const VSL_NAMESPACE::ClearValue opposite = is_signed ? VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearUint{}}
                                                             : VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearInt{}};
        EXPECT_NO_THROW((VSL_NAMESPACE::command::RenderPassBegin(target, {correct})));
        EXPECT_NO_THROW((VSL_NAMESPACE::command::RenderPassBegin::WithClears(target, {{0, correct}})));
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{}})), std::invalid_argument);
        EXPECT_THROW((VSL_NAMESPACE::command::RenderPassBegin(target, {opposite})), std::invalid_argument);
    }
    INSTANTIATE_TEST_SUITE_P(PackedFormats, PackedIntegerClearTest, testing::Values(
        PackedIntegerClearCase{vk::Format::eA2R10G10B10SintPack32, true},
        PackedIntegerClearCase{vk::Format::eA2B10G10R10SintPack32, true},
        PackedIntegerClearCase{vk::Format::eA2R10G10B10UintPack32, false},
        PackedIntegerClearCase{vk::Format::eA2B10G10R10UintPack32, false}),
        [](const testing::TestParamInfo<PackedIntegerClearCase>& info) { return vk::to_string(info.param.format); });
}

namespace {
    class PackedIntegerClearVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest,
                                                   public testing::WithParamInterface<PackedIntegerClearCase> {
        void SetUp() override {
            require_validation = true;
            VulkanIntegrationTest::SetUp();
        }
    };
    TEST_P(PackedIntegerClearVulkanIntegrationTest, IntegerClearReachesEveryPackedPixel) {
        const auto [format, is_signed] = GetParam();
        const auto flags = physical.data_->device.getFormatProperties(format).optimalTilingFeatures;
        if (!(flags & vk::FormatFeatureFlagBits::eColorAttachment))
            GTEST_SKIP() << "Packed format is not a color attachment on this GPU: " << vk::to_string(format);
        if (!host_coherent_available())
            GTEST_SKIP() << "Host coherent readback memory unavailable";
        const VSL_NAMESPACE::data_format::Format packed(static_cast<VSL_NAMESPACE::data_format::FormatTypes>(format), 4);
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::COLOR_ATTACHMENT | VSL_NAMESPACE::ImageType::TRANSFER_SRC> image(device, 2, 2, packed);
        VSL_NAMESPACE::RenderPassDescription description;
        description.add_color({.format = packed, .final_layout = VSL_NAMESPACE::ImageLayout::GENERAL});
        VSL_NAMESPACE::RenderPass pass(device, description);
        VSL_NAMESPACE::FrameBuffer framebuffer(pass, 2, 2, {image});
        const VSL_NAMESPACE::ClearValue clear = is_signed ? VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearInt{-7, 15, -1, 1}}
                                                          : VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearUint{17, 31, 47, 2}};
        const VSL_NAMESPACE::command::RenderPassBegin begin(framebuffer, {clear});
        VSL_NAMESPACE::StagingBuffer<VSL_NAMESPACE::MemoryType::TRANSFER_DESTINATION> destination(device, std::size_t{16});
        auto manager = make_manager();
        VSL_NAMESPACE::Completion completion;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << begin << VSL_NAMESPACE::command::RenderPassEnd{};
            auto buffer = manager.get_buffer();
            auto command = buffer.data_->command_buffers.at(buffer.get_current_buffer_idx());
            // 形式の判定から独立したnativeコピーで、クリア後のpackedビット列を検証する。
            vk::ImageMemoryBarrier image_barrier{
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral, .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image.data_->image, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
            command.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
                                    {}, {}, {}, image_barrier);
            vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                     .imageExtent = {2, 2, 1}};
            command.copyImageToBuffer(image.data_->image, vk::ImageLayout::eTransferSrcOptimal, destination.data_->buffer, copy);
            vk::BufferMemoryBarrier host_visibility{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = destination.data_->buffer, .offset = 0, .size = 16};
            command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
                                    {}, {}, host_visibility, {});
            completion = phase.completion();
            ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
        }
        ASSERT_TRUE(completion.wait());
        const std::uint32_t red = is_signed ? (std::uint32_t(-7) & 0x3ffu) : 17u;
        const std::uint32_t green = is_signed ? 15u : 31u;
        const std::uint32_t blue = is_signed ? 0x3ffu : 47u;
        const std::uint32_t alpha = is_signed ? 1u : 2u;
        const bool red_first = format == vk::Format::eA2B10G10R10UintPack32 || format == vk::Format::eA2B10G10R10SintPack32;
        const auto expected = (alpha << 30) | (green << 10) | (red_first ? red | (blue << 20) : blue | (red << 20));
        const auto mapped = destination.data();
        for (std::size_t i = 0; i < 4; ++i) {
            std::uint32_t pixel;
            std::memcpy(&pixel, static_cast<const std::byte*>(mapped.data) + i * sizeof(pixel), sizeof(pixel));
            EXPECT_EQ(pixel, expected) << "pixel " << i;
        }
    }
    INSTANTIATE_TEST_SUITE_P(PackedFormats, PackedIntegerClearVulkanIntegrationTest, testing::Values(
        PackedIntegerClearCase{vk::Format::eA2R10G10B10SintPack32, true},
        PackedIntegerClearCase{vk::Format::eA2B10G10R10SintPack32, true},
        PackedIntegerClearCase{vk::Format::eA2R10G10B10UintPack32, false},
        PackedIntegerClearCase{vk::Format::eA2B10G10R10UintPack32, false}),
        [](const testing::TestParamInfo<PackedIntegerClearCase>& info) { return vk::to_string(info.param.format); });
}

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
        auto target = std::make_shared<vsl::impl::FrameBufferImpl>();
        target->render_pass = std::move(pass.data_);
        target->swap_chain_framebuffers.emplace_back();
        target->extent = vk::Extent2D{2, 2};
        return {std::move(target)};
    }
    TEST_P(LegacyClearFormatVulkanIntegrationTest, ExplicitRgbMustNotBypassIntegerOrDepthClearValidation) {
        const auto format = GetParam();
        const bool depth = format == vk::Format::eD32Sfloat;
        vsl::RenderPassDescription description;
        vsl::AttachmentDescription attachment{.format = {static_cast<vsl::data_format::FormatTypes>(format), 4},
                                               .final_layout = depth ? vsl::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL : vsl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL};
        if (depth)
            description.set_depth_stencil(attachment);
        else
            description.add_color(attachment);
        vsl::RenderPass pass(device, description);
        auto target = legacy_metadata_target(pass);
        const vsl::command::RenderPassBegin defaults(pass, target);
        ASSERT_EQ(defaults.clear_values.size(), 1u);
        const auto kind = defaults.clear_values[0].index();
        EXPECT_NE(kind, 0u);
        EXPECT_FALSE(defaults.clear_color);
        EXPECT_FALSE(defaults.target_index);
        float red = 0.25f, green = 0.5f, blue = 0.75f;
        const vsl::VSLDRGBColorAccessor<float> color{red, green, blue};
        EXPECT_THROW((vsl::command::RenderPassBegin(pass, target, color)), std::invalid_argument);
        EXPECT_EQ(vsl::command::RenderPassBegin(pass, target).clear_values[0].index(), kind);
    }
    INSTANTIATE_TEST_SUITE_P(NumericFormats, LegacyClearFormatVulkanIntegrationTest,
                            testing::Values(vk::Format::eR32Sint, vk::Format::eR32Uint, vk::Format::eD32Sfloat),
                            [](const testing::TestParamInfo<vk::Format>& info) { return vk::to_string(info.param); });
    TEST_F(LegacyClearVulkanIntegrationTest, ExplicitRgbIsRejectedWhenThereAreNoAttachments) {
        vsl::RenderPass pass(device, vsl::RenderPassDescription{});
        auto target = legacy_metadata_target(pass);
        EXPECT_TRUE(vsl::command::RenderPassBegin(pass, target).clear_values.empty());
        float red = 1, green = 0, blue = 0;
        const vsl::VSLDRGBColorAccessor<float> color{red, green, blue};
        EXPECT_THROW((vsl::command::RenderPassBegin(pass, target, color)), std::invalid_argument);
    }
    TEST_F(LegacyClearVulkanIntegrationTest, PickingOverloadAlsoRejectsIntegerRgbClearsAndPreservesTypedDefaults) {
        for (const auto format : {vk::Format::eR32Sint, vk::Format::eR32Uint}) {
            SCOPED_TRACE(vk::to_string(format));
            vsl::SwapchainAccessor chain;
            chain.data_ = std::make_shared<vsl::impl::SwapchainImpl>();
            chain.data_->device = device.data_;
            chain.data_->extent = chain.data_->swap_chain_extent = vk::Extent2D{2, 2};
            chain.data_->swap_chain_image_format = format;
            vsl::IDPickingRenderPass pass(chain);
            auto target = legacy_metadata_target(pass);
            const vsl::command::IDPickingRenderPassBegin defaults(pass, target);
            ASSERT_EQ(defaults.clear_values.size(), 2u);
            EXPECT_EQ(defaults.clear_values[0].index(), format == vk::Format::eR32Sint ? 1u : 2u);
            EXPECT_TRUE(std::holds_alternative<vsl::ClearUint>(defaults.clear_values[1]));
            float red = 1, green = 0, blue = 0;
            const vsl::VSLDRGBColorAccessor<float> color{red, green, blue};
            EXPECT_THROW((vsl::command::IDPickingRenderPassBegin(pass, target, color)), std::invalid_argument);
        }
    }
    TEST_F(LegacyClearVulkanIntegrationTest, PickingRgbPreservesTheIdAttachmentDefaultAndZeroAlpha) {
        vsl::SwapchainAccessor chain;
        chain.data_ = std::make_shared<vsl::impl::SwapchainImpl>();
        chain.data_->device = device.data_;
        chain.data_->extent = chain.data_->swap_chain_extent = vk::Extent2D{2, 2};
        chain.data_->swap_chain_image_format = vk::Format::eR8G8B8A8Unorm;
        vsl::IDPickingRenderPass pass(chain);
        auto target = legacy_metadata_target(pass);
        float red = 0.25f, green = 0.5f, blue = 0.75f;
        const vsl::VSLDRGBColorAccessor<float> color{red, green, blue};
        const vsl::command::IDPickingRenderPassBegin begin(pass, target, color);
        ASSERT_EQ(begin.clear_values.size(), 2u);
        ASSERT_TRUE(std::holds_alternative<vsl::ClearColor>(begin.clear_values[0]));
        const auto clear = std::get<vsl::ClearColor>(begin.clear_values[0]);
        EXPECT_EQ(clear.r, red);
        EXPECT_EQ(clear.g, green);
        EXPECT_EQ(clear.b, blue);
        EXPECT_EQ(clear.a, 0.f);
        ASSERT_TRUE(std::holds_alternative<vsl::ClearUint>(begin.clear_values[1]));
        const auto id = std::get<vsl::ClearUint>(begin.clear_values[1]);
        EXPECT_EQ(id.r, 0u);
        EXPECT_EQ(id.g, 0u);
        EXPECT_EQ(id.b, 0u);
        EXPECT_EQ(id.a, 0u);
        ASSERT_TRUE(begin.clear_color);
        EXPECT_EQ(begin.clear_color->r, red);
        EXPECT_FALSE(begin.target_index);
    }
    TEST_F(LegacyClearVulkanIntegrationTest, ClearColorForAnUnclearedAttachmentKeepsTheCommonConstructorsIgnoredValueSemantics) {
        vsl::RenderPassDescription description;
        description.add_color({.format = vsl::data_format::UNSIGNED_INT, .load = vsl::LoadOp::DONT_CARE});
        vsl::RenderPass pass(device, description);
        auto target = legacy_metadata_target(pass);
        float red = 1, green = 0, blue = 0;
        const vsl::VSLDRGBColorAccessor<float> color{red, green, blue};
        EXPECT_NO_THROW((vsl::command::RenderPassBegin(target, {vsl::ClearColor{red, green, blue, 0}})));
        EXPECT_NO_THROW((vsl::command::RenderPassBegin(pass, target, color)));
    }
    TEST_F(RenderPassBeginVulkanIntegrationTest, LegacyRgbClearPreservesZeroAlphaAndReachesEveryPixel) {
        float red = 0.25f, green = 0.5f, blue = 0.75f;
        const vsl::VSLDRGBColorAccessor<float> color_value{red, green, blue};
        vsl::command::RenderPassBegin begin(*pass, *framebuffer, color_value);
        ASSERT_TRUE(begin.clear_color);
        EXPECT_EQ(begin.clear_color->r, red);
        EXPECT_FALSE(begin.target_index);
        ASSERT_EQ(begin.clear_values.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<vsl::ClearColor>(begin.clear_values[0]));
        EXPECT_EQ(std::get<vsl::ClearColor>(begin.clear_values[0]).a, 0.f);
        auto manager = make_manager();
        {
            vsl::ComputePhase phase(manager);
            phase << begin << vsl::command::RenderPassEnd{};
        }
        const auto bytes = vsl_test::read_image(manager, *color, vsl::ImageLayout::GENERAL);
        ASSERT_EQ(bytes.size(), WIDTH * HEIGHT * 4u);
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
            // UNORM8の丸めを1段階だけ許容し、alphaの既存値0は厳密一致で確認する。
            EXPECT_NEAR(std::to_integer<int>(bytes[offset]), 64, 1);
            EXPECT_NEAR(std::to_integer<int>(bytes[offset + 1]), 128, 1);
            EXPECT_NEAR(std::to_integer<int>(bytes[offset + 2]), 191, 1);
            EXPECT_EQ(bytes[offset + 3], std::byte{0});
        }
    }
} // namespace
