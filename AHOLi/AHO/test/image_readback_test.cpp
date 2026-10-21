#ifdef AHO_GPU_TESTS
#include "support/render_fixture.hpp"
#include <AHO/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <cstring>
namespace {
    class ImageReadbackVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool ImageReadbackVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    namespace pl = VSL_NAMESPACE::pipeline_layout;

    TEST_F(ImageReadbackVulkanIntegrationTest, ExistingCommandsReadPixelsWithinTheDrawingPhase) {
        auto manager = make_manager();
        VSL_NAMESPACE::StagingBuffer<VSL_NAMESPACE::MemoryType::TransferDestination> destination(device, 3 * 2 * 4);
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << *pipeline << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ChangeImageBarrier(*color, VSL_NAMESPACE::ImageLayout::General, VSL_NAMESPACE::ImageLayout::TransferSrcOptimal)
                  << VSL_NAMESPACE::command::CopyImageToBuffer(destination, *color, VSL_NAMESPACE::ImageLayout::TransferSrcOptimal, {1, 2, 3, 2}, true)
                  << VSL_NAMESPACE::command::ChangeImageBarrier(*color, VSL_NAMESPACE::ImageLayout::TransferSrcOptimal, VSL_NAMESPACE::ImageLayout::General);
        }
        device._data->device.waitIdle();
        auto expected = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::General, {1, 2, 3, 2}).read();
        auto mapped = destination.data(expected.bytes.size());
        EXPECT_EQ(std::memcmp(mapped.data, expected.bytes.data(), expected.bytes.size()), 0);
        EXPECT_NEAR(std::to_integer<int>(expected.bytes[0]), 64, 1);
        EXPECT_EQ(expected.bytes[3], std::byte{255});
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, ScreenshotAndRegionReadbackUsePublicApiAndRestoreLayout) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << *pipeline << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback screenshot(manager, *color, VSL_NAMESPACE::ImageLayout::General);
        ASSERT_TRUE(screenshot.wait());
        EXPECT_TRUE(screenshot.ready());
        const auto pixels = screenshot.read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.rowStride, 32u);
        EXPECT_EQ(pixels.format, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA);
        ASSERT_EQ(pixels.bytes.size(), 256u);
        for (std::size_t i = 0; i < pixels.bytes.size(); i += 4) {
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i]), 64, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i + 1]), 128, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i + 2]), 191, 1);
            EXPECT_EQ(pixels.bytes[i + 3], std::byte{255});
        }
        // 再度Generalから読み戻し、最初の要求でレイアウトが復元されたことを確かめる。
        auto region = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::General, {1, 2, 3, 2}).read();
        EXPECT_EQ(region.rowStride, 12u);
        ASSERT_EQ(region.bytes.size(), 24u);
        EXPECT_TRUE(std::equal(region.bytes.begin(), region.bytes.end(), pixels.bytes.begin()));
        auto sharedRequest = screenshot;
        EXPECT_EQ(sharedRequest.read().bytes, pixels.bytes);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, PickingUsesTwoGenericColorOutputsWithoutSpecialPassOrCommand) {
        if (!physical._data->features->independentBlend)
            GTEST_SKIP() << "independentBlend required for mixed outputs";
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::ColorAttachment | VSL_NAMESPACE::ImageType::TransferSrc> ids(
            device, 8, 8, VSL_NAMESPACE::data_format::UnsignedInt);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.addColor({.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA});
        desc.addColor({.format = VSL_NAMESPACE::data_format::UnsignedInt});
        VSL_NAMESPACE::RenderPass picking(device, desc);
        VSL_NAMESPACE::FrameBuffer target(picking, 8, 8, {*color, ids});
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment> shader(device, vsl_test::shader_path("picking.frag.spv"));
        pl::ColorBlend blends(
            std::vector<pl::ColorBlendAttachment>{{}, {.enabled = false, .writeMask = pl::ColorWriteMask::R}});
        VSL_NAMESPACE::PipelineLayout settings(device, pl::ShaderGroup("picking", {*vertex, shader}), pl::VertexInput{},
                                               blends, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None},
                                               pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport(description),
                                               VSL_NAMESPACE::Scissor(description));
        VSL_NAMESPACE::GraphicsPipeline draw(settings, picking);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{}, VSL_NAMESPACE::ClearUint{7}}) << draw
                  << VSL_NAMESPACE::Viewport(description) << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3}
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback reader(manager);
        ASSERT_TRUE(reader.capture(ids, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal, {4, 4, 1, 1}));
        auto picked = reader.read();
        std::uint32_t id = 0;
        ASSERT_EQ(picked.bytes.size(), sizeof(id));
        std::memcpy(&id, picked.bytes.data(), sizeof(id));
        EXPECT_EQ(id, 42u);
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal));
        auto screen = reader.read();
        EXPECT_EQ(screen.format, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA);
        EXPECT_NEAR(std::to_integer<int>(screen.bytes[0]), 64, 1);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, SignedAndUnsignedClearsHaveNativePixelRepresentation) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager);
        for (bool signedValue : {false, true}) {
            ColorImage image(device, 8, 8, signedValue ? VSL_NAMESPACE::data_format::Int : VSL_NAMESPACE::data_format::UnsignedInt);
            VSL_NAMESPACE::RenderPassDescription desc;
            desc.addColor({.format = image.getFormat()});
            VSL_NAMESPACE::RenderPass pass(device, desc);
            VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {image});
            VSL_NAMESPACE::ClearValue clear =
                signedValue ? VSL_NAMESPACE::ClearValue(VSL_NAMESPACE::ClearInt{-17}) : VSL_NAMESPACE::ClearValue(VSL_NAMESPACE::ClearUint{123456});
            {
                auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
                phase << VSL_NAMESPACE::command::RenderPassBegin(target, {clear}) << VSL_NAMESPACE::command::RenderPassEnd{};
            }
            ASSERT_TRUE(reader.capture(image, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal));
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
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::DepthStencilAttachment> depth(device, 8, 8, VSL_NAMESPACE::data_format::Depth32Float);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.addColor({.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA});
        desc.setDepthStencil({.format = VSL_NAMESPACE::data_format::Depth32Float,
                              .finalLayout = VSL_NAMESPACE::ImageLayout::DepthStencilAttachmentOptimal});
        VSL_NAMESPACE::RenderPass pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {*color, depth});
        VSL_NAMESPACE::PipelineLayout settings(device, pl::ShaderGroup("depth", {*vertex, *fragment}), pl::VertexInput{},
                                               pl::ColorBlend{}, pl::DepthStencil{.depthCompare = pl::CompareOp::Never},
                                               pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None},
                                               pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport(description),
                                               VSL_NAMESPACE::Scissor(description));
        VSL_NAMESPACE::GraphicsPipeline draw(settings, pass);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target,
                                                             {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}, VSL_NAMESPACE::ClearDepthStencil{1, 0}})
                  << draw << VSL_NAMESPACE::Viewport(description) << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3}
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto pixels = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal).read();
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        EXPECT_EQ(pixels.bytes[1], std::byte{0});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RejectsInvalidRegionsAndUnsupportedSourceContracts) {
        auto manager = make_manager();
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::General, {7, 0, 2, 1})),
                     std::out_of_range);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::General, {0, 0, 1, 1, 1})),
                     std::out_of_range);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::General, {1, 0, 0, 0})),
                     std::invalid_argument);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::Undefined)), std::invalid_argument);
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::ColorAttachment> noTransfer(device, 8, 8);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, noTransfer, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal)),
                     std::invalid_argument);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RequestKeepsSourceAliveAfterCallerReleasesImage) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 1, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        // 描画側の参照を解放する前に完了を待ち、読み戻し側だけで画像の寿命を維持できるか確かめる。
        device._data->device.waitIdle();
        std::weak_ptr<VSL_NAMESPACE::_impl::Image_impl> source = color->_data;
        auto request = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::General);
        framebuffer.reset();
        description.images.clear();
        color.reset();
        EXPECT_FALSE(source.expired());
        EXPECT_EQ(request.read().bytes[1], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, MultipleSubpassesShareGenericBeginAndNextCommands) {
        ColorImage second(device, 8, 8, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.addColor({.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA});
        auto other = desc.addAttachment({.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA});
        desc.subpasses.push_back({.colors = {{other}}, .preserve = {0}});
        desc.dependencies.push_back({.source = 0, .destination = 1});
        desc.dependencies.push_back({.source = 1, .destination = VSL_NAMESPACE::ExternalSubpass});
        VSL_NAMESPACE::RenderPass pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {*color, second});
        VSL_NAMESPACE::GraphicsPipeline first(*layout, pass, 0), last(*layout, pass, 1);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target) << first << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::NextSubpass{} << last
                  << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto a = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal).read();
        auto b = AHO_NAMESPACE::ImageReadback(manager, second, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal).read();
        EXPECT_EQ(a.bytes, b.bytes);
        EXPECT_NEAR(std::to_integer<int>(b.bytes[0]), 64, 1);
    }
} // namespace

namespace {
    TEST_F(ImageReadbackVulkanIntegrationTest, MultisampleResolveIsConfiguredByAttachmentReferences) {
        if (!(physical._data->props->limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4))
            GTEST_SKIP() << "Four-sample color attachments unsupported";
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::ColorAttachment> multisampled(
            device, 8, 8, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA, 1, VSL_NAMESPACE::SampleCount::Four);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.addColor({.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA, .samples = VSL_NAMESPACE::SampleCount::Four});
        const auto resolve = desc.addAttachment(
            {.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA, .load = VSL_NAMESPACE::LoadOp::DontCare});
        desc.subpasses[0].resolves = {{resolve}};
        VSL_NAMESPACE::RenderPass pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {multisampled, *color});
        VSL_NAMESPACE::PipelineLayout settings(device, pl::ShaderGroup("resolve", {*vertex, *fragment}), pl::VertexInput{},
                                               pl::ColorBlend{}, pl::InputAssembly{},
                                               pl::Rasterization{pl::CullingMode::None},
                                               pl::Multisample{VSL_NAMESPACE::SampleCount::Four}, pl::DynamicState{},
                                               VSL_NAMESPACE::Viewport(description), VSL_NAMESPACE::Scissor(description));
        VSL_NAMESPACE::GraphicsPipeline draw(settings, pass);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target) << draw << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto pixels = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal).read();
        EXPECT_NEAR(std::to_integer<int>(pixels.bytes[0]), 64, 1);
        EXPECT_EQ(pixels.bytes[3], std::byte{255});
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, multisampled, VSL_NAMESPACE::ImageLayout::ColorAttachmentOptimal)),
                     std::invalid_argument);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, ArrayLayerReadbackAndLoadPassPreserveTexels) {
        ColorImage array(device, 8, 8, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA, 2);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.addColor(
            {.format = VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA, .finalLayout = VSL_NAMESPACE::ImageLayout::General});
        VSL_NAMESPACE::RenderPass clearPass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(clearPass, 8, 8, {array}, 2);
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{0, 0, 1, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        // 同じコマンドバッファを再利用するため、前の送信の完了を待つ。
        device._data->device.waitIdle();
        desc.attachments[0].load = VSL_NAMESPACE::LoadOp::Load;
        desc.attachments[0].initialLayout = VSL_NAMESPACE::ImageLayout::General;
        VSL_NAMESPACE::RenderPass loadPass(device, desc);
        VSL_NAMESPACE::FrameBuffer loaded(loadPass, 8, 8, {array}, 2);
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(loaded) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto request = AHO_NAMESPACE::ImageReadback(manager, array, VSL_NAMESPACE::ImageLayout::General, {0, 0, 8, 8, 1});
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
    TEST_F(ImageReadbackVulkanIntegrationTest, ReusesGPUCapacityAndCallerCPUStorageAcrossFrames) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager, 8 * 8 * 4);
        EXPECT_FALSE(reader.ready());
        EXPECT_THROW(reader.read(), std::logic_error);
        AHO_NAMESPACE::ImagePixels pixels;
        pixels.bytes.resize(8 * 8 * 4);
        const auto storage = pixels.bytes.data();
        for (int frame = 0; frame < 24; ++frame) {
            const auto green = frame % 2 == 0;
            {
                auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
                phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, green ? 1.f : 0.f, green ? 0.f : 1.f, 1}})
                      << VSL_NAMESPACE::command::RenderPassEnd{};
            }
            ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::General));
            reader.read(pixels);
            EXPECT_EQ(reader.capacity(), 8u * 8u * 4u);
            EXPECT_EQ(pixels.bytes.data(), storage);
            EXPECT_EQ(pixels.bytes[1], green ? std::byte{255} : std::byte{0});
            EXPECT_EQ(pixels.bytes[2], green ? std::byte{0} : std::byte{255});
        }
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::General, {3, 4, 1, 1}));
        reader.read(pixels);
        EXPECT_EQ(pixels.bytes.size(), 4u);
        EXPECT_EQ(pixels.width, 1u);
        EXPECT_EQ(reader.capacity(), 256u);
        EXPECT_EQ(pixels.bytes.data(), storage);
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, CapacityGrowsForLargerImagesAndIsKeptForSmallerCaptures) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager, 4);
        ColorImage larger(device, 16, 12, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.addColor({.format = larger.getFormat(), .finalLayout = VSL_NAMESPACE::ImageLayout::General});
        VSL_NAMESPACE::RenderPass renderPass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(renderPass, 16, 12, {larger});
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        ASSERT_TRUE(reader.capture(larger, VSL_NAMESPACE::ImageLayout::General));
        auto pixels = reader.read();
        EXPECT_EQ(pixels.bytes.size(), 16u * 12u * 4u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        const auto capacity = reader.capacity();
        ASSERT_TRUE(reader.capture(larger, VSL_NAMESPACE::ImageLayout::General, {1, 2, 2, 3}));
        pixels = reader.read();
        EXPECT_EQ(pixels.width, 2u);
        EXPECT_EQ(pixels.height, 3u);
        EXPECT_EQ(pixels.rowStride, 8u);
        EXPECT_EQ(reader.capacity(), capacity);
        EXPECT_THROW(reader.capture(larger, VSL_NAMESPACE::ImageLayout::General, {16, 0, 1, 1}), std::out_of_range);
        EXPECT_EQ(reader.read().bytes, pixels.bytes);
    }

    class ImageReadbackBusyVulkanIntegrationTest : public ImageReadbackVulkanIntegrationTest {
        bool require_timeline_semaphore() const override;
    };

    bool ImageReadbackBusyVulkanIntegrationTest::require_timeline_semaphore() const {
        return true;
    }

    TEST_F(ImageReadbackBusyVulkanIntegrationTest, BusyCaptureDoesNotWaitOrOverwriteAnOutstandingTransfer) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        device._data->device.waitIdle();
        AHO_NAMESPACE::ImageReadback reader(manager, 256);
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
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::General));
        EXPECT_FALSE(reader.ready());
        EXPECT_FALSE(reader.wait(0));
        EXPECT_FALSE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::General, {0, 0, 1, 1}));
        unblock.release();
        auto pixels = reader.read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.bytes.size(), 256u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RecordsColorAndIdWithAttachmentKeysInOneSubmission) {
        auto manager = make_manager();
        ColorImage ids(device, width, height, VSL_NAMESPACE::data_format::UnsignedInt);
        VSL_NAMESPACE::RenderPassDescription description;
        auto objectId = description.addColor({.format = ids.getFormat(), .finalLayout = VSL_NAMESPACE::ImageLayout::General});
        auto colorId = description.addColor({.format = color->getFormat(), .finalLayout = VSL_NAMESPACE::ImageLayout::General});
        VSL_NAMESPACE::RenderPass pass(device, description);
        auto target = VSL_NAMESPACE::FrameBuffer::fromAttachments(pass, width, height, {{colorId, *color}, {objectId, ids}});
        AHO_NAMESPACE::ImageReadback reader(manager, 0, 2);
        VSL_NAMESPACE::ComputePhase phase(manager);
        phase << VSL_NAMESPACE::command::RenderPassBegin::withClears(target, {
                                                                                 {colorId, VSL_NAMESPACE::ClearColor{1, 0, 0, 1}},
                                                                                 {objectId, VSL_NAMESPACE::ClearUint{77}},
                                                                             })
              << VSL_NAMESPACE::command::RenderPassEnd{};
        auto idTicket = reader.record(phase, ids, VSL_NAMESPACE::ImageLayout::General, {1, 2, 1, 1}, 123);
        auto colorTicket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::General, {}, 123);
        ASSERT_TRUE(idTicket);
        ASSERT_TRUE(colorTicket);
        EXPECT_EQ(idTicket->status(), VSL_NAMESPACE::Completion::Status::Pending);
        EXPECT_FALSE(idTicket->wait(0));
        EXPECT_THROW(idTicket->read(0), std::logic_error);
        EXPECT_EQ(idTicket->info().frameId, 123u);
        EXPECT_EQ(idTicket->info().region.x, 1u);
        EXPECT_EQ(idTicket->info().region.y, 2u);
        EXPECT_EQ(idTicket->info().rowStride, 4u);
        EXPECT_EQ(idTicket->info().byteSize, 4u);
        EXPECT_EQ(colorTicket->info().region.width, width);
        EXPECT_GT(colorTicket->info().requestId, idTicket->info().requestId);
        EXPECT_EQ(reader.capacity(), width * height * 4u + 4u);
        ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
        auto selected = idTicket->read();
        std::uint32_t id = 0;
        std::memcpy(&id, selected.bytes.data(), sizeof(id));
        EXPECT_EQ(id, 77u);
        EXPECT_EQ(colorTicket->read().bytes[0], std::byte{255});
        EXPECT_THROW(reader.record(phase, ids, VSL_NAMESPACE::ImageLayout::General), std::logic_error);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, CompletedTicketsRetainIndependentResultsAndOccupyTheirSlots) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager, 0, 2);
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto red = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {}, 7);
        ASSERT_TRUE(red);
        ASSERT_TRUE(red->wait());
        const auto firstId = red->info().requestId;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 0, 1, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto blue = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {}, 8);
        ASSERT_TRUE(blue);
        ASSERT_TRUE(blue->wait());
        EXPECT_FALSE(reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General));
        EXPECT_EQ(red->read().bytes[0], std::byte{255});
        EXPECT_EQ(red->read().bytes[2], std::byte{0});
        EXPECT_EQ(blue->read().bytes[0], std::byte{0});
        EXPECT_EQ(blue->read().bytes[2], std::byte{255});
        EXPECT_EQ(red->info().frameId, 7u);
        EXPECT_EQ(blue->info().frameId, 8u);
        red.reset();
        auto next = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {0, 0, 1, 1}, 9);
        ASSERT_TRUE(next);
        EXPECT_GT(next->info().requestId, firstId);
        EXPECT_EQ(next->read().bytes[2], std::byte{255});
        EXPECT_EQ(blue->info().frameId, 8u);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, TicketRemainsReadableAfterWorkerAndProducerAreReleased) {
        std::optional<AHO_NAMESPACE::ImageReadbackTicket> ticket;
        {
            auto manager = make_manager();
            AHO_NAMESPACE::ImageReadback reader(manager);
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 1, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
            ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::General, {}, 42);
            ASSERT_TRUE(ticket);
            phase.submit();
        }
        ASSERT_TRUE(ticket->wait());
        EXPECT_EQ(ticket->info().frameId, 42u);
        EXPECT_EQ(ticket->read().bytes[1], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, CancelledAndUnwoundRecordingsCanReleaseAndReuseTheirSlot) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager);
        std::optional<AHO_NAMESPACE::ImageReadbackTicket> ticket;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            try {
                VSL_NAMESPACE::ComputePhase phase(manager);
                phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << VSL_NAMESPACE::command::RenderPassEnd{};
                ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::General);
                ASSERT_TRUE(ticket);
                if (attempt == 0)
                    phase.cancel();
                else
                    throw std::runtime_error("cancel recording");
            } catch (const std::runtime_error&) {
            }
            EXPECT_EQ(ticket->status(), VSL_NAMESPACE::Completion::Status::Cancelled);
            EXPECT_FALSE(ticket->wait(0));
            EXPECT_THROW(ticket->read(0), std::logic_error);
            ticket.reset();
        }
        VSL_NAMESPACE::ComputePhase phase(manager);
        phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 0, 1, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::General);
        ASSERT_TRUE(ticket);
        phase.submit();
        EXPECT_EQ(ticket->read().bytes[2], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, ReusesReleasedTicketSlotAcrossRecordedFrames) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager);
        AHO_NAMESPACE::ImagePixels pixels;
        const std::byte* storage = nullptr;
        for (unsigned frame = 0; frame < 24; ++frame) {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
            auto ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::General, {}, frame);
            ASSERT_TRUE(ticket);
            phase.submit();
            ticket->read(pixels);
            EXPECT_EQ(ticket->info().frameId, frame);
            EXPECT_EQ(pixels.bytes[0], std::byte{255});
            EXPECT_EQ(reader.capacity(), width * height * 4u);
            if (storage)
                EXPECT_EQ(pixels.bytes.data(), storage);
            storage = pixels.bytes.data();
        }
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, 0, 0)), std::invalid_argument);
    }
    TEST_F(ImageReadbackBusyVulkanIntegrationTest, MultipleTicketSlotsSubmitWithoutWaitingAndRejectExcessRequests) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        device._data->device.waitIdle();
        AHO_NAMESPACE::ImageReadback reader(manager, 256, 2);
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
        auto first = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {}, 10);
        auto second = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {0, 0, 1, 1}, 11);
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_FALSE(first->ready());
        EXPECT_FALSE(second->ready());
        EXPECT_FALSE(first->wait(0));
        EXPECT_FALSE(second->wait(0));
        EXPECT_FALSE(reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {}, 12));
        EXPECT_EQ(first->info().frameId, 10u);
        EXPECT_EQ(second->info().frameId, 11u);
        unblock.release();
        auto pixels = first->read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.bytes.size(), 256u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        EXPECT_EQ(second->read().bytes.size(), 4u);
        EXPECT_EQ(second->info().frameId, 11u);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, DiscardLatestSwitchesFromConvenienceCaptureToTicketWithoutRebuildingWorker) {
        auto manager = make_manager();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback reader(manager);
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::General));
        ASSERT_TRUE(reader.wait());
        EXPECT_FALSE(reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General));
        reader.discardLatest();
        EXPECT_FALSE(reader.ready());
        EXPECT_THROW(reader.read(), std::logic_error);
        auto ticket = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General);
        ASSERT_TRUE(ticket);
        EXPECT_EQ(ticket->read().bytes[0], std::byte{255});
        EXPECT_EQ(reader.capacity(), width * height * 4u);
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, UsesAnAvailableLargeSlotBeforeGrowingAnotherSlot) {
        auto manager = make_manager();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback reader(manager, 4, 2);
        auto small = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General, {0, 0, 1, 1});
        ASSERT_TRUE(small);
        ASSERT_TRUE(small->wait());
        auto large = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General);
        ASSERT_TRUE(large);
        ASSERT_TRUE(large->wait());
        EXPECT_EQ(reader.capacity(), 4u + width * height * 4u);
        small.reset();
        large.reset();
        auto next = reader.captureTicket(*color, VSL_NAMESPACE::ImageLayout::General);
        ASSERT_TRUE(next);
        EXPECT_TRUE(next->wait());
        EXPECT_EQ(reader.capacity(), 4u + width * height * 4u);
    }

} // namespace
#endif
