#ifdef AHO_GPU_TESTS
#include "support/render_fixture.hpp"
#include <AHO/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <cstring>
namespace {
    class ImageReadbackVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    namespace pl = vsl::pipeline_layout;

    TEST_F(ImageReadbackVulkanIntegrationTest, ExistingCommandsReadPixelsWithinTheDrawingPhase) {
        auto manager = make_manager();
        vsl::StagingBuffer<vsl::MemoryType::TransferDestination> destination(device, 3 * 2 * 4);
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer) << *pipeline << vsl::Viewport(description)
                  << vsl::Scissor(description) << vsl::command::Draw{3} << vsl::command::RenderPassEnd{}
                  << vsl::command::ChangeImageBarrier(*color, vsl::ImageLayout::General, vsl::ImageLayout::TransferSrcOptimal)
                  << vsl::command::CopyImageToBuffer(destination, *color, vsl::ImageLayout::TransferSrcOptimal, {1, 2, 3, 2}, true)
                  << vsl::command::ChangeImageBarrier(*color, vsl::ImageLayout::TransferSrcOptimal, vsl::ImageLayout::General);
        }
        device._data->device.waitIdle();
        auto expected = aho::ImageReadback(manager, *color, vsl::ImageLayout::General, {1, 2, 3, 2}).read();
        auto mapped = destination.data(expected.bytes.size());
        EXPECT_EQ(std::memcmp(mapped.data, expected.bytes.data(), expected.bytes.size()), 0);
        EXPECT_NEAR(std::to_integer<int>(expected.bytes[0]), 64, 1);
        EXPECT_EQ(expected.bytes[3], std::byte{255});
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, ScreenshotAndRegionReadbackUsePublicApiAndRestoreLayout) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer) << *pipeline << vsl::Viewport(description)
                  << vsl::Scissor(description) << vsl::command::Draw{3} << vsl::command::RenderPassEnd{};
        }
        aho::ImageReadback screenshot(manager, *color, vsl::ImageLayout::General);
        ASSERT_TRUE(screenshot.wait());
        EXPECT_TRUE(screenshot.ready());
        const auto pixels = screenshot.read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.rowStride, 32u);
        EXPECT_EQ(pixels.format, vsl::data_format::UnsignedNormalized8RGBA);
        ASSERT_EQ(pixels.bytes.size(), 256u);
        for (std::size_t i = 0; i < pixels.bytes.size(); i += 4) {
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i]), 64, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i + 1]), 128, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i + 2]), 191, 1);
            EXPECT_EQ(pixels.bytes[i + 3], std::byte{255});
        }
        // A second copy from General verifies restoration after the first request.
        auto region = aho::ImageReadback(manager, *color, vsl::ImageLayout::General, {1, 2, 3, 2}).read();
        EXPECT_EQ(region.rowStride, 12u);
        ASSERT_EQ(region.bytes.size(), 24u);
        EXPECT_TRUE(std::equal(region.bytes.begin(), region.bytes.end(), pixels.bytes.begin()));
        auto sharedRequest = screenshot;
        EXPECT_EQ(sharedRequest.read().bytes, pixels.bytes);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, PickingUsesTwoGenericColorOutputsWithoutSpecialPassOrCommand) {
        if (!physical._data->features->independentBlend)
            GTEST_SKIP() << "independentBlend required for mixed outputs";
        vsl::Image<vsl::ImageType::ColorAttachment | vsl::ImageType::TransferSrc> ids(
            device, 8, 8, vsl::data_format::UnsignedInt);
        vsl::RenderPassDescription desc;
        desc.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA});
        desc.addColor({.format = vsl::data_format::UnsignedInt});
        vsl::RenderPass picking(device, desc);
        vsl::FrameBuffer target(picking, 8, 8, {*color, ids});
        vsl::Shader<vsl::ShaderType::Fragment> shader(device, vsl_test::shader_path("picking.frag.spv"));
        pl::ColorBlend blends(
            std::vector<pl::ColorBlendAttachment>{{}, {.enabled = false, .writeMask = pl::ColorWriteMask::R}});
        vsl::PipelineLayout settings(device, pl::ShaderGroup("picking", {*vertex, shader}), pl::VertexInput{},
                                     blends, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None},
                                     pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description),
                                     vsl::Scissor(description));
        vsl::GraphicsPipeline draw(settings, picking);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target, {vsl::ClearColor{}, vsl::ClearUint{7}}) << draw
                  << vsl::Viewport(description) << vsl::Scissor(description) << vsl::command::Draw{3}
                  << vsl::command::RenderPassEnd{};
        }
        aho::ImageReadback reader(manager);
        ASSERT_TRUE(reader.capture(ids, vsl::ImageLayout::ColorAttachmentOptimal, {4, 4, 1, 1}));
        auto picked = reader.read();
        std::uint32_t id = 0;
        ASSERT_EQ(picked.bytes.size(), sizeof(id));
        std::memcpy(&id, picked.bytes.data(), sizeof(id));
        EXPECT_EQ(id, 42u);
        ASSERT_TRUE(reader.capture(*color, vsl::ImageLayout::ColorAttachmentOptimal));
        auto screen = reader.read();
        EXPECT_EQ(screen.format, vsl::data_format::UnsignedNormalized8RGBA);
        EXPECT_NEAR(std::to_integer<int>(screen.bytes[0]), 64, 1);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, SignedAndUnsignedClearsHaveNativePixelRepresentation) {
        auto manager = make_manager();
        aho::ImageReadback reader(manager);
        for (bool signedValue : {false, true}) {
            ColorImage image(device, 8, 8, signedValue ? vsl::data_format::Int : vsl::data_format::UnsignedInt);
            vsl::RenderPassDescription desc;
            desc.addColor({.format = image.getFormat()});
            vsl::RenderPass pass(device, desc);
            vsl::FrameBuffer target(pass, 8, 8, {image});
            vsl::ClearValue clear =
                signedValue ? vsl::ClearValue(vsl::ClearInt{-17}) : vsl::ClearValue(vsl::ClearUint{123456});
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                phase << vsl::command::RenderPassBegin(target, {clear}) << vsl::command::RenderPassEnd{};
            }
            ASSERT_TRUE(reader.capture(image, vsl::ImageLayout::ColorAttachmentOptimal));
            auto pixels = reader.read();
            EXPECT_EQ(pixels.format, image.getFormat());
            for (std::size_t i = 0; i < pixels.bytes.size(); i += 4) {
                std::int32_t value;
                std::memcpy(&value, pixels.bytes.data() + i, 4);
                EXPECT_EQ(value, signedValue ? -17 : 123456);
            }
        }
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, DepthAttachmentCanRejectFragmentsThroughPublicPipelineState) {
        const auto props = physical._data->device.getFormatProperties(vk::Format::eD32Sfloat);
        if (!(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment))
            GTEST_SKIP() << "D32 depth unsupported";
        vsl::Image<vsl::ImageType::DepthStencilAttachment> depth(device, 8, 8, vsl::data_format::Depth32Float);
        vsl::RenderPassDescription desc;
        desc.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA});
        desc.setDepthStencil({.format = vsl::data_format::Depth32Float,
                              .finalLayout = vsl::ImageLayout::DepthStencilAttachmentOptimal});
        vsl::RenderPass pass(device, desc);
        vsl::FrameBuffer target(pass, 8, 8, {*color, depth});
        vsl::PipelineLayout settings(device, pl::ShaderGroup("depth", {*vertex, *fragment}), pl::VertexInput{},
                                     pl::ColorBlend{}, pl::DepthStencil{.depthCompare = pl::CompareOp::Never},
                                     pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None},
                                     pl::Multisample{}, pl::DynamicState{}, vsl::Viewport(description),
                                     vsl::Scissor(description));
        vsl::GraphicsPipeline draw(settings, pass);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target,
                                                   {vsl::ClearColor{1, 0, 0, 1}, vsl::ClearDepthStencil{1, 0}})
                  << draw << vsl::Viewport(description) << vsl::Scissor(description) << vsl::command::Draw{3}
                  << vsl::command::RenderPassEnd{};
        }
        auto pixels = aho::ImageReadback(manager, *color, vsl::ImageLayout::ColorAttachmentOptimal).read();
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        EXPECT_EQ(pixels.bytes[1], std::byte{0});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RejectsInvalidRegionsAndUnsupportedSourceContracts) {
        auto manager = make_manager();
        EXPECT_THROW((aho::ImageReadback(manager, *color, vsl::ImageLayout::General, {7, 0, 2, 1})),
                     std::out_of_range);
        EXPECT_THROW((aho::ImageReadback(manager, *color, vsl::ImageLayout::General, {0, 0, 1, 1, 1})),
                     std::out_of_range);
        EXPECT_THROW((aho::ImageReadback(manager, *color, vsl::ImageLayout::General, {1, 0, 0, 0})),
                     std::invalid_argument);
        EXPECT_THROW((aho::ImageReadback(manager, *color, vsl::ImageLayout::Undefined)), std::invalid_argument);
        vsl::Image<vsl::ImageType::ColorAttachment> noTransfer(device, 8, 8);
        EXPECT_THROW((aho::ImageReadback(manager, noTransfer, vsl::ImageLayout::ColorAttachmentOptimal)),
                     std::invalid_argument);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RequestKeepsSourceAliveAfterCallerReleasesImage) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer, {vsl::ClearColor{0, 1, 0, 1}})
                  << vsl::command::RenderPassEnd{};
        }
        // Drawing owns the framebuffer until its submission completes. Readback owns its own transfer resources.
        device._data->device.waitIdle();
        std::weak_ptr<vsl::_impl::Image_impl> source = color->_data;
        auto request = aho::ImageReadback(manager, *color, vsl::ImageLayout::General);
        framebuffer.reset();
        description.images.clear();
        color.reset();
        EXPECT_FALSE(source.expired());
        EXPECT_EQ(request.read().bytes[1], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, MultipleSubpassesShareGenericBeginAndNextCommands) {
        ColorImage second(device, 8, 8, vsl::data_format::UnsignedNormalized8RGBA);
        vsl::RenderPassDescription desc;
        desc.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA});
        auto other = desc.addAttachment({.format = vsl::data_format::UnsignedNormalized8RGBA});
        desc.subpasses.push_back({.colors = {{other}}, .preserve = {0}});
        desc.dependencies.push_back({.source = 0, .destination = 1});
        desc.dependencies.push_back({.source = 1, .destination = vsl::ExternalSubpass});
        vsl::RenderPass pass(device, desc);
        vsl::FrameBuffer target(pass, 8, 8, {*color, second});
        vsl::GraphicsPipeline first(*layout, pass, 0), last(*layout, pass, 1);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target) << first << vsl::Viewport(description)
                  << vsl::Scissor(description) << vsl::command::Draw{3} << vsl::command::NextSubpass{} << last
                  << vsl::command::Draw{3} << vsl::command::RenderPassEnd{};
        }
        auto a = aho::ImageReadback(manager, *color, vsl::ImageLayout::ColorAttachmentOptimal).read();
        auto b = aho::ImageReadback(manager, second, vsl::ImageLayout::ColorAttachmentOptimal).read();
        EXPECT_EQ(a.bytes, b.bytes);
        EXPECT_NEAR(std::to_integer<int>(b.bytes[0]), 64, 1);
    }
} // namespace

namespace {
    TEST_F(ImageReadbackVulkanIntegrationTest, MultisampleResolveIsConfiguredByAttachmentReferences) {
        if (!(physical._data->props->limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4))
            GTEST_SKIP() << "Four-sample color attachments unsupported";
        vsl::Image<vsl::ImageType::ColorAttachment> multisampled(
            device, 8, 8, vsl::data_format::UnsignedNormalized8RGBA, 1, vsl::SampleCount::Four);
        vsl::RenderPassDescription desc;
        desc.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA, .samples = vsl::SampleCount::Four});
        const auto resolve = desc.addAttachment(
            {.format = vsl::data_format::UnsignedNormalized8RGBA, .load = vsl::LoadOp::DontCare});
        desc.subpasses[0].resolves = {{resolve}};
        vsl::RenderPass pass(device, desc);
        vsl::FrameBuffer target(pass, 8, 8, {multisampled, *color});
        vsl::PipelineLayout settings(device, pl::ShaderGroup("resolve", {*vertex, *fragment}), pl::VertexInput{},
                                     pl::ColorBlend{}, pl::InputAssembly{},
                                     pl::Rasterization{pl::CullingMode::None},
                                     pl::Multisample{vsl::SampleCount::Four}, pl::DynamicState{},
                                     vsl::Viewport(description), vsl::Scissor(description));
        vsl::GraphicsPipeline draw(settings, pass);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target) << draw << vsl::Viewport(description)
                  << vsl::Scissor(description) << vsl::command::Draw{3} << vsl::command::RenderPassEnd{};
        }
        auto pixels = aho::ImageReadback(manager, *color, vsl::ImageLayout::ColorAttachmentOptimal).read();
        EXPECT_NEAR(std::to_integer<int>(pixels.bytes[0]), 64, 1);
        EXPECT_EQ(pixels.bytes[3], std::byte{255});
        EXPECT_THROW((aho::ImageReadback(manager, multisampled, vsl::ImageLayout::ColorAttachmentOptimal)),
                     std::invalid_argument);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, ArrayLayerReadbackAndLoadPassPreserveTexels) {
        ColorImage array(device, 8, 8, vsl::data_format::UnsignedNormalized8RGBA, 2);
        vsl::RenderPassDescription desc;
        desc.addColor(
            {.format = vsl::data_format::UnsignedNormalized8RGBA, .finalLayout = vsl::ImageLayout::General});
        vsl::RenderPass clearPass(device, desc);
        vsl::FrameBuffer target(clearPass, 8, 8, {array}, 2);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target, {vsl::ClearColor{0, 0, 1, 1}})
                  << vsl::command::RenderPassEnd{};
        }
        // ComputePhase reuses the manager command buffer; complete the previous submission first.
        device._data->device.waitIdle();
        desc.attachments[0].load = vsl::LoadOp::Load;
        desc.attachments[0].initialLayout = vsl::ImageLayout::General;
        vsl::RenderPass loadPass(device, desc);
        vsl::FrameBuffer loaded(loadPass, 8, 8, {array}, 2);
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(loaded) << vsl::command::RenderPassEnd{};
        }
        auto request = aho::ImageReadback(manager, array, vsl::ImageLayout::General, {0, 0, 8, 8, 1});
        ASSERT_TRUE(request.wait());
        auto pixels = request.read(0);
        ASSERT_EQ(pixels.bytes.size(), 256u);
        for (std::size_t i = 0; i < pixels.bytes.size(); i += 4) {
            EXPECT_EQ(pixels.bytes[i], std::byte{0});
            EXPECT_EQ(pixels.bytes[i + 2], std::byte{255});
            EXPECT_EQ(pixels.bytes[i + 3], std::byte{255});
        }
    }
} // namespace

namespace {
    TEST_F(ImageReadbackVulkanIntegrationTest, ReusesGpuCapacityAndCallerCpuStorageAcrossFrames) {
        auto manager = make_manager();
        aho::ImageReadback reader(manager, 8 * 8 * 4);
        EXPECT_FALSE(reader.ready());
        EXPECT_THROW(reader.read(), std::logic_error);
        aho::ImagePixels pixels;
        pixels.bytes.resize(8 * 8 * 4);
        const auto storage = pixels.bytes.data();
        for (int frame = 0; frame < 24; ++frame) {
            const auto green = frame % 2 == 0;
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                phase << vsl::command::RenderPassBegin(*framebuffer, {vsl::ClearColor{0, green ? 1.f : 0.f, green ? 0.f : 1.f, 1}})
                      << vsl::command::RenderPassEnd{};
            }
            ASSERT_TRUE(reader.capture(*color, vsl::ImageLayout::General));
            reader.read(pixels);
            EXPECT_EQ(reader.capacity(), 8u * 8u * 4u);
            EXPECT_EQ(pixels.bytes.data(), storage);
            EXPECT_EQ(pixels.bytes[1], green ? std::byte{255} : std::byte{0});
            EXPECT_EQ(pixels.bytes[2], green ? std::byte{0} : std::byte{255});
        }
        ASSERT_TRUE(reader.capture(*color, vsl::ImageLayout::General, {3, 4, 1, 1}));
        reader.read(pixels);
        EXPECT_EQ(pixels.bytes.size(), 4u);
        EXPECT_EQ(pixels.width, 1u);
        EXPECT_EQ(reader.capacity(), 256u);
        EXPECT_EQ(pixels.bytes.data(), storage);
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, CapacityGrowsForLargerImagesAndIsKeptForSmallerCaptures) {
        auto manager = make_manager();
        aho::ImageReadback reader(manager, 4);
        ColorImage larger(device, 16, 12, vsl::data_format::UnsignedNormalized8RGBA);
        vsl::RenderPassDescription desc;
        desc.addColor({.format = larger.getFormat(), .finalLayout = vsl::ImageLayout::General});
        vsl::RenderPass renderPass(device, desc);
        vsl::FrameBuffer target(renderPass, 16, 12, {larger});
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(target, {vsl::ClearColor{1, 0, 0, 1}})
                  << vsl::command::RenderPassEnd{};
        }
        ASSERT_TRUE(reader.capture(larger, vsl::ImageLayout::General));
        auto pixels = reader.read();
        EXPECT_EQ(pixels.bytes.size(), 16u * 12u * 4u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        const auto capacity = reader.capacity();
        ASSERT_TRUE(reader.capture(larger, vsl::ImageLayout::General, {1, 2, 2, 3}));
        pixels = reader.read();
        EXPECT_EQ(pixels.width, 2u);
        EXPECT_EQ(pixels.height, 3u);
        EXPECT_EQ(pixels.rowStride, 8u);
        EXPECT_EQ(reader.capacity(), capacity);
        EXPECT_THROW(reader.capture(larger, vsl::ImageLayout::General, {16, 0, 1, 1}), std::out_of_range);
        EXPECT_EQ(reader.read().bytes, pixels.bytes);
    }

    class ImageReadbackBusyVulkanIntegrationTest : public ImageReadbackVulkanIntegrationTest {
        bool require_timeline_semaphore() const override { return true; }
    };

    TEST_F(ImageReadbackBusyVulkanIntegrationTest, BusyCaptureDoesNotWaitOrOverwriteAnOutstandingTransfer) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            phase << vsl::command::RenderPassBegin(*framebuffer, {vsl::ClearColor{1, 0, 0, 1}})
                  << vsl::command::RenderPassEnd{};
        }
        device._data->device.waitIdle();
        aho::ImageReadback reader(manager, 256);
        vk::SemaphoreTypeCreateInfo type{.semaphoreType = vk::SemaphoreType::eTimeline};
        auto semaphore = device._data->device.createSemaphoreUnique({.pNext = &type});
        const auto signal = reinterpret_cast<PFN_vkSignalSemaphoreKHR>(
            vkGetDeviceProcAddr(device._data->device, "vkSignalSemaphoreKHR"));
        ASSERT_NE(signal, nullptr);
        struct Unblock {
            vk::Device device;
            vk::Semaphore semaphore;
            PFN_vkSignalSemaphoreKHR signal;
            bool released = false;
            void release() {
                if (released)
                    return;
                VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
                info.semaphore = semaphore;
                info.value = 1;
                (void)signal(device, &info);
                released = true;
            }
            ~Unblock() {
                release();
                device.waitIdle();
            }
        } unblock{device._data->device, *semaphore, signal};
        const std::uint64_t value = 1;
        vk::TimelineSemaphoreSubmitInfo timeline{.waitSemaphoreValueCount = 1, .pWaitSemaphoreValues = &value};
        const vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eAllCommands;
        manager._data->graphicsQueue.submit(vk::SubmitInfo{
            .pNext = &timeline, .waitSemaphoreCount = 1, .pWaitSemaphores = &*semaphore, .pWaitDstStageMask = &stage});
        ASSERT_TRUE(reader.capture(*color, vsl::ImageLayout::General));
        EXPECT_FALSE(reader.ready());
        EXPECT_FALSE(reader.wait(0));
        EXPECT_FALSE(reader.capture(*color, vsl::ImageLayout::General, {0, 0, 1, 1}));
        unblock.release();
        auto pixels = reader.read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.bytes.size(), 256u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
    }
}
#endif
