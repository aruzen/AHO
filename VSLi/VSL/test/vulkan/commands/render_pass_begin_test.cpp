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
    struct PackedIntegerClearCase {
        vk::Format format;
        bool isSigned;
    };
    void PrintTo(const PackedIntegerClearCase& value, std::ostream* stream) {
        *stream << vk::to_string(value.format);
    }
    class PackedIntegerClearTest : public testing::TestWithParam<PackedIntegerClearCase> {};
    VSL_NAMESPACE::FrameBufferAccessor metadata_framebuffer(vk::Format format) {
        auto description = std::make_shared<VSL_NAMESPACE::RenderPassDescription>();
        description->addColor({.format = {static_cast<VSL_NAMESPACE::data_format::___FormatTypes>(format), 4}});
        auto pass = std::make_shared<VSL_NAMESPACE::_impl::RenderPass_impl>();
        pass->description = std::move(description);
        auto target = std::make_shared<VSL_NAMESPACE::_impl::FrameBuffer_impl>();
        target->renderPass = std::move(pass);
        target->swapChainFramebuffers.emplace_back();
        target->extent = vk::Extent2D{1, 1};
        return {std::move(target)};
    }
    TEST_P(PackedIntegerClearTest, DefaultsAndExplicitClearsUseTheIntegerNumericType) {
        const auto [format, isSigned] = GetParam();
        auto target = metadata_framebuffer(format);
        const VSL_NAMESPACE::command::RenderPassBegin defaults(target);
        ASSERT_EQ(defaults.clearValues.size(), 1u);
        EXPECT_EQ(std::holds_alternative<VSL_NAMESPACE::ClearInt>(defaults.clearValues[0]), isSigned);
        EXPECT_EQ(std::holds_alternative<VSL_NAMESPACE::ClearUint>(defaults.clearValues[0]), !isSigned);
        const VSL_NAMESPACE::ClearValue correct = isSigned ? VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearInt{-7, 15, -1, 1}}
                                                            : VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearUint{17, 31, 47, 2}};
        const VSL_NAMESPACE::ClearValue opposite = isSigned ? VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearUint{}}
                                                             : VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearInt{}};
        EXPECT_NO_THROW((VSL_NAMESPACE::command::RenderPassBegin(target, {correct})));
        EXPECT_NO_THROW((VSL_NAMESPACE::command::RenderPassBegin::withClears(target, {{0, correct}})));
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
        const auto [format, isSigned] = GetParam();
        const auto flags = physical._data->device.getFormatProperties(format).optimalTilingFeatures;
        if (!(flags & vk::FormatFeatureFlagBits::eColorAttachment))
            GTEST_SKIP() << "Packed format is not a color attachment on this GPU: " << vk::to_string(format);
        if (!host_coherent_available())
            GTEST_SKIP() << "Host coherent readback memory unavailable";
        const VSL_NAMESPACE::data_format::___Format packed(static_cast<VSL_NAMESPACE::data_format::___FormatTypes>(format), 4);
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::ColorAttachment | VSL_NAMESPACE::ImageType::TransferSrc> image(device, 2, 2, packed);
        VSL_NAMESPACE::RenderPassDescription description;
        description.addColor({.format = packed, .finalLayout = VSL_NAMESPACE::ImageLayout::General});
        VSL_NAMESPACE::RenderPass pass(device, description);
        VSL_NAMESPACE::FrameBuffer framebuffer(pass, 2, 2, {image});
        const VSL_NAMESPACE::ClearValue clear = isSigned ? VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearInt{-7, 15, -1, 1}}
                                                          : VSL_NAMESPACE::ClearValue{VSL_NAMESPACE::ClearUint{17, 31, 47, 2}};
        const VSL_NAMESPACE::command::RenderPassBegin begin(framebuffer, {clear});
        VSL_NAMESPACE::StagingBuffer<VSL_NAMESPACE::MemoryType::TransferDestination> destination(device, std::size_t{16});
        auto manager = make_manager();
        VSL_NAMESPACE::Completion completion;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << begin << VSL_NAMESPACE::command::RenderPassEnd{};
            auto buffer = manager.getBuffer();
            auto command = buffer._data->commandBuffers.at(buffer.getCurrentBufferIdx());
            // 形式の判定から独立したnativeコピーで、クリア後のpackedビット列を検証する。
            vk::ImageMemoryBarrier imageBarrier{
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral, .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image._data->image, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
            command.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
                                    {}, {}, {}, imageBarrier);
            vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                     .imageExtent = {2, 2, 1}};
            command.copyImageToBuffer(image._data->image, vk::ImageLayout::eTransferSrcOptimal, destination._data->buffer, copy);
            vk::BufferMemoryBarrier hostVisibility{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = destination._data->buffer, .offset = 0, .size = 16};
            command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
                                    {}, {}, hostVisibility, {});
            completion = phase.completion();
            ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
        }
        ASSERT_TRUE(completion.wait());
        const std::uint32_t red = isSigned ? (std::uint32_t(-7) & 0x3ffu) : 17u;
        const std::uint32_t green = isSigned ? 15u : 31u;
        const std::uint32_t blue = isSigned ? 0x3ffu : 47u;
        const std::uint32_t alpha = isSigned ? 1u : 2u;
        const bool redFirst = format == vk::Format::eA2B10G10R10UintPack32 || format == vk::Format::eA2B10G10R10SintPack32;
        const auto expected = (alpha << 30) | (green << 10) | (redFirst ? red | (blue << 20) : blue | (red << 20));
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
