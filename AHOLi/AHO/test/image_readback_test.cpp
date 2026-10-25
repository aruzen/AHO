#ifdef AHO_GPU_TESTS
#include "support/render_fixture.hpp"
#include <AHO/image_readback.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <cstring>
namespace {
    class ImageReadbackVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    class ImageReadbackBusyVulkanIntegrationTest : public ImageReadbackVulkanIntegrationTest {
        bool require_timeline_semaphore() const override;
    };

    bool ImageReadbackVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    namespace pl = VSL_NAMESPACE::pipeline_layout;

    TEST_F(ImageReadbackVulkanIntegrationTest, ExistingCommandsReadPixelsWithinTheDrawingPhase) {
        auto manager = make_manager();
        VSL_NAMESPACE::StagingBuffer<VSL_NAMESPACE::MemoryType::TRANSFER_DESTINATION> destination(device, 3 * 2 * 4);
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << *pipeline << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{}
                  << VSL_NAMESPACE::command::ChangeImageBarrier(*color, VSL_NAMESPACE::ImageLayout::GENERAL, VSL_NAMESPACE::ImageLayout::TRANSFER_SRC_OPTIMAL)
                  << VSL_NAMESPACE::command::CopyImageToBuffer(destination, *color, VSL_NAMESPACE::ImageLayout::TRANSFER_SRC_OPTIMAL, {1, 2, 3, 2}, true)
                  << VSL_NAMESPACE::command::ChangeImageBarrier(*color, VSL_NAMESPACE::ImageLayout::TRANSFER_SRC_OPTIMAL, VSL_NAMESPACE::ImageLayout::GENERAL);
        }
        device.data_->device.waitIdle();
        auto expected = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {1, 2, 3, 2}).read();
        auto mapped = destination.data(expected.bytes.size());
        EXPECT_EQ(std::memcmp(mapped.data, expected.bytes.data(), expected.bytes.size()), 0);
        EXPECT_NEAR(std::to_integer<int>(expected.bytes[0]), 64, 1);
        EXPECT_EQ(expected.bytes[3], std::byte{255});
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, ScreenshotAndRegionReadbackUsePublicApiAndRestoreLayout) {
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << *pipeline << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback screenshot(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
        ASSERT_TRUE(screenshot.wait());
        EXPECT_TRUE(screenshot.ready());
        const auto pixels = screenshot.read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.row_stride, 32u);
        EXPECT_EQ(pixels.format, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA);
        ASSERT_EQ(pixels.bytes.size(), 256u);
        for (std::size_t i = 0; i < pixels.bytes.size(); i += 4) {
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i]), 64, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i + 1]), 128, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels.bytes[i + 2]), 191, 1);
            EXPECT_EQ(pixels.bytes[i + 3], std::byte{255});
        }
        // 再度Generalから読み戻し、最初の要求でレイアウトが復元されたことを確かめる。
        auto region = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {1, 2, 3, 2}).read();
        EXPECT_EQ(region.row_stride, 12u);
        ASSERT_EQ(region.bytes.size(), 24u);
        EXPECT_TRUE(std::equal(region.bytes.begin(), region.bytes.end(), pixels.bytes.begin()));
        auto shared_request = screenshot;
        EXPECT_EQ(shared_request.read().bytes, pixels.bytes);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, PickingUsesTwoGenericColorOutputsWithoutSpecialPassOrCommand) {
        if (!physical.data_->features->independentBlend)
            GTEST_SKIP() << "independentBlend required for mixed outputs";
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::COLOR_ATTACHMENT | VSL_NAMESPACE::ImageType::TRANSFER_SRC> ids(
            device, 8, 8, VSL_NAMESPACE::data_format::UNSIGNED_INT);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.add_color({.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA});
        desc.add_color({.format = VSL_NAMESPACE::data_format::UNSIGNED_INT});
        VSL_NAMESPACE::RenderPass picking(device, desc);
        VSL_NAMESPACE::FrameBuffer target(picking, 8, 8, {*color, ids});
        VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::FRAGMENT> shader(device, vsl_test::shader_path("picking.frag.spv"));
        pl::ColorBlend blends(
            std::vector<pl::ColorBlendAttachment>{{}, {.enabled = false, .write_mask = pl::ColorWriteMask::R}});
        VSL_NAMESPACE::PipelineLayout settings(device, pl::ShaderGroup("picking", {*vertex, shader}), pl::VertexInput{},
                                               blends, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                                               pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport(description),
                                               VSL_NAMESPACE::Scissor(description));
        VSL_NAMESPACE::GraphicsPipeline draw(settings, picking);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{}, VSL_NAMESPACE::ClearUint{7}}) << draw
                  << VSL_NAMESPACE::Viewport(description) << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3}
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback reader(manager);
        ASSERT_TRUE(reader.capture(ids, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL, {4, 4, 1, 1}));
        auto picked = reader.read();
        std::uint32_t id = 0;
        ASSERT_EQ(picked.bytes.size(), sizeof(id));
        std::memcpy(&id, picked.bytes.data(), sizeof(id));
        EXPECT_EQ(id, 42u);
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL));
        auto screen = reader.read();
        EXPECT_EQ(screen.format, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA);
        EXPECT_NEAR(std::to_integer<int>(screen.bytes[0]), 64, 1);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, SignedAndUnsignedClearsHaveNativePixelRepresentation) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager);
        for (bool signed_value : {false, true}) {
            ColorImage image(device, 8, 8, signed_value ? VSL_NAMESPACE::data_format::INT : VSL_NAMESPACE::data_format::UNSIGNED_INT);
            VSL_NAMESPACE::RenderPassDescription desc;
            desc.add_color({.format = image.get_format()});
            VSL_NAMESPACE::RenderPass pass(device, desc);
            VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {image});
            VSL_NAMESPACE::ClearValue clear =
                signed_value ? VSL_NAMESPACE::ClearValue(VSL_NAMESPACE::ClearInt{-17}) : VSL_NAMESPACE::ClearValue(VSL_NAMESPACE::ClearUint{123456});
            {
                auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
                phase << VSL_NAMESPACE::command::RenderPassBegin(target, {clear}) << VSL_NAMESPACE::command::RenderPassEnd{};
            }
            ASSERT_TRUE(reader.capture(image, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL));
            auto pixels = reader.read();
            EXPECT_EQ(pixels.format, image.get_format());
            for (std::size_t i = 0; i < pixels.bytes.size(); i += 4) {
                std::int32_t value;
                std::memcpy(&value, pixels.bytes.data() + i, 4);
                EXPECT_EQ(value, signed_value ? -17 : 123456);
            }
        }
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, DepthAttachmentCanRejectFragmentsThroughPublicPipelineState) {
        const auto props = physical.data_->device.getFormatProperties(vk::Format::eD32Sfloat);
        if (!(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment))
            GTEST_SKIP() << "D32 depth unsupported";
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::DEPTH_STENCIL_ATTACHMENT> depth(device, 8, 8, VSL_NAMESPACE::data_format::DEPTH32_FLOAT);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.add_color({.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA});
        desc.set_depth_stencil({.format = VSL_NAMESPACE::data_format::DEPTH32_FLOAT,
                                .final_layout = VSL_NAMESPACE::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
        VSL_NAMESPACE::RenderPass pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {*color, depth});
        VSL_NAMESPACE::PipelineLayout settings(device, pl::ShaderGroup("depth", {*vertex, *fragment}), pl::VertexInput{},
                                               pl::ColorBlend{}, pl::DepthStencil{.depth_compare = pl::CompareOp::NEVER},
                                               pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE},
                                               pl::Multisample{}, pl::DynamicState{}, VSL_NAMESPACE::Viewport(description),
                                               VSL_NAMESPACE::Scissor(description));
        VSL_NAMESPACE::GraphicsPipeline draw(settings, pass);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target,
                                                             {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}, VSL_NAMESPACE::ClearDepthStencil{1, 0}})
                  << draw << VSL_NAMESPACE::Viewport(description) << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3}
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto pixels = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL).read();
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        EXPECT_EQ(pixels.bytes[1], std::byte{0});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RejectsInvalidRegionsAndUnsupportedSourceContracts) {
        auto manager = make_manager();
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {7, 0, 2, 1})),
                     std::out_of_range);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {0, 0, 1, 1, 1})),
                     std::out_of_range);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {1, 0, 0, 0})),
                     std::invalid_argument);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::UNDEFINED)), std::invalid_argument);
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::COLOR_ATTACHMENT> no_transfer(device, 8, 8);
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, no_transfer, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)),
                     std::invalid_argument);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RequestKeepsSourceAliveAfterCallerReleasesImage) {
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 1, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        // 描画側の参照を解放する前に完了を待ち、読み戻し側だけで画像の寿命を維持できるか確かめる。
        device.data_->device.waitIdle();
        std::weak_ptr<VSL_NAMESPACE::impl::ImageImpl> source = color->data_;
        auto request = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
        framebuffer.reset();
        description.images.clear();
        color.reset();
        EXPECT_FALSE(source.expired());
        EXPECT_EQ(request.read().bytes[1], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, MultipleSubpassesShareGenericBeginAndNextCommands) {
        ColorImage second(device, 8, 8, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.add_color({.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA});
        auto other = desc.add_attachment({.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA});
        desc.subpasses.push_back({.colors = {{other}}, .preserve = {0}});
        desc.dependencies.push_back({.source = 0, .destination = 1});
        desc.dependencies.push_back({.source = 1, .destination = VSL_NAMESPACE::EXTERNAL_SUBPASS});
        VSL_NAMESPACE::RenderPass pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {*color, second});
        VSL_NAMESPACE::GraphicsPipeline first(*layout, pass, 0), last(*layout, pass, 1);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target) << first << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::NextSubpass{} << last
                  << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto a = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL).read();
        auto b = AHO_NAMESPACE::ImageReadback(manager, second, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL).read();
        EXPECT_EQ(a.bytes, b.bytes);
        EXPECT_NEAR(std::to_integer<int>(b.bytes[0]), 64, 1);
    }
} // namespace

namespace {
    TEST_F(ImageReadbackVulkanIntegrationTest, MultisampleResolveIsConfiguredByAttachmentReferences) {
        if (!(physical.data_->props->limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4))
            GTEST_SKIP() << "Four-sample color attachments unsupported";
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::COLOR_ATTACHMENT> multisampled(
            device, 8, 8, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA, 1, VSL_NAMESPACE::SampleCount::FOUR);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.add_color({.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA, .samples = VSL_NAMESPACE::SampleCount::FOUR});
        const auto resolve = desc.add_attachment(
            {.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA, .load = VSL_NAMESPACE::LoadOp::DONT_CARE});
        desc.subpasses[0].resolves = {{resolve}};
        VSL_NAMESPACE::RenderPass pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {multisampled, *color});
        VSL_NAMESPACE::PipelineLayout settings(device, pl::ShaderGroup("resolve", {*vertex, *fragment}), pl::VertexInput{},
                                               pl::ColorBlend{}, pl::InputAssembly{},
                                               pl::Rasterization{pl::CullingMode::NONE},
                                               pl::Multisample{VSL_NAMESPACE::SampleCount::FOUR}, pl::DynamicState{},
                                               VSL_NAMESPACE::Viewport(description), VSL_NAMESPACE::Scissor(description));
        VSL_NAMESPACE::GraphicsPipeline draw(settings, pass);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target) << draw << VSL_NAMESPACE::Viewport(description)
                  << VSL_NAMESPACE::Scissor(description) << VSL_NAMESPACE::command::Draw{3} << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto pixels = AHO_NAMESPACE::ImageReadback(manager, *color, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL).read();
        EXPECT_NEAR(std::to_integer<int>(pixels.bytes[0]), 64, 1);
        EXPECT_EQ(pixels.bytes[3], std::byte{255});
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, multisampled, VSL_NAMESPACE::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)),
                     std::invalid_argument);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, ArrayLayerReadbackAndLoadPassPreserveTexels) {
        ColorImage array(device, 8, 8, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA, 2);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.add_color(
            {.format = VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA, .final_layout = VSL_NAMESPACE::ImageLayout::GENERAL});
        VSL_NAMESPACE::RenderPass clear_pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(clear_pass, 8, 8, {array}, 2);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{0, 0, 1, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        // 同じコマンドバッファを再利用するため、前の送信の完了を待つ。
        device.data_->device.waitIdle();
        desc.attachments[0].load = VSL_NAMESPACE::LoadOp::LOAD;
        desc.attachments[0].initial_layout = VSL_NAMESPACE::ImageLayout::GENERAL;
        VSL_NAMESPACE::RenderPass load_pass(device, desc);
        VSL_NAMESPACE::FrameBuffer loaded(load_pass, 8, 8, {array}, 2);
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(loaded) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto request = AHO_NAMESPACE::ImageReadback(manager, array, VSL_NAMESPACE::ImageLayout::GENERAL, {0, 0, 8, 8, 1});
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
                auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
                phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, green ? 1.f : 0.f, green ? 0.f : 1.f, 1}})
                      << VSL_NAMESPACE::command::RenderPassEnd{};
            }
            ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::GENERAL));
            reader.read(pixels);
            EXPECT_EQ(reader.capacity(), 8u * 8u * 4u);
            EXPECT_EQ(pixels.bytes.data(), storage);
            EXPECT_EQ(pixels.bytes[1], green ? std::byte{255} : std::byte{0});
            EXPECT_EQ(pixels.bytes[2], green ? std::byte{0} : std::byte{255});
        }
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {3, 4, 1, 1}));
        reader.read(pixels);
        EXPECT_EQ(pixels.bytes.size(), 4u);
        EXPECT_EQ(pixels.width, 1u);
        EXPECT_EQ(reader.capacity(), 256u);
        EXPECT_EQ(pixels.bytes.data(), storage);
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, CapacityGrowsForLargerImagesAndIsKeptForSmallerCaptures) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager, 4);
        ColorImage larger(device, 16, 12, VSL_NAMESPACE::data_format::UNSIGNED_NORMALIZED8_RGBA);
        VSL_NAMESPACE::RenderPassDescription desc;
        desc.add_color({.format = larger.get_format(), .final_layout = VSL_NAMESPACE::ImageLayout::GENERAL});
        VSL_NAMESPACE::RenderPass render_pass(device, desc);
        VSL_NAMESPACE::FrameBuffer target(render_pass, 16, 12, {larger});
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(target, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        ASSERT_TRUE(reader.capture(larger, VSL_NAMESPACE::ImageLayout::GENERAL));
        auto pixels = reader.read();
        EXPECT_EQ(pixels.bytes.size(), 16u * 12u * 4u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        const auto capacity = reader.capacity();
        ASSERT_TRUE(reader.capture(larger, VSL_NAMESPACE::ImageLayout::GENERAL, {1, 2, 2, 3}));
        pixels = reader.read();
        EXPECT_EQ(pixels.width, 2u);
        EXPECT_EQ(pixels.height, 3u);
        EXPECT_EQ(pixels.row_stride, 8u);
        EXPECT_EQ(reader.capacity(), capacity);
        EXPECT_THROW(reader.capture(larger, VSL_NAMESPACE::ImageLayout::GENERAL, {16, 0, 1, 1}), std::out_of_range);
        EXPECT_EQ(reader.read().bytes, pixels.bytes);
    }

    bool ImageReadbackBusyVulkanIntegrationTest::require_timeline_semaphore() const {
        return true;
    }

    TEST_F(ImageReadbackBusyVulkanIntegrationTest, BusyCaptureDoesNotWaitOrOverwriteAnOutstandingTransfer) {
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        device.data_->device.waitIdle();
        AHO_NAMESPACE::ImageReadback reader(manager, 256);
        vk::SemaphoreTypeCreateInfo type{.semaphoreType = vk::SemaphoreType::eTimeline};
        auto semaphore = device.data_->device.createSemaphoreUnique({.pNext = &type});
        const auto signal = reinterpret_cast<PFN_vkSignalSemaphoreKHR>(
            vkGetDeviceProcAddr(device.data_->device, "vkSignalSemaphoreKHR"));
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
        } unblock{device.data_->device, *semaphore, signal};
        const std::uint64_t value = 1;
        vk::TimelineSemaphoreSubmitInfo timeline{.waitSemaphoreValueCount = 1, .pWaitSemaphoreValues = &value};
        const vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eAllCommands;
        manager.data_->graphics_queue.submit(vk::SubmitInfo{
            .pNext = &timeline, .waitSemaphoreCount = 1, .pWaitSemaphores = &*semaphore, .pWaitDstStageMask = &stage});
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::GENERAL));
        EXPECT_FALSE(reader.ready());
        EXPECT_FALSE(reader.wait(0));
        EXPECT_FALSE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {0, 0, 1, 1}));
        unblock.release();
        auto pixels = reader.read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.bytes.size(), 256u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, RecordsColorAndIdWithAttachmentKeysInOneSubmission) {
        auto manager = make_manager();
        ColorImage ids(device, WIDTH, HEIGHT, VSL_NAMESPACE::data_format::UNSIGNED_INT);
        VSL_NAMESPACE::RenderPassDescription description;
        auto object_id = description.add_color({.format = ids.get_format(), .final_layout = VSL_NAMESPACE::ImageLayout::GENERAL});
        auto color_id = description.add_color({.format = color->get_format(), .final_layout = VSL_NAMESPACE::ImageLayout::GENERAL});
        VSL_NAMESPACE::RenderPass pass(device, description);
        auto target = VSL_NAMESPACE::FrameBuffer::FromAttachments(pass, WIDTH, HEIGHT, {{color_id, *color}, {object_id, ids}});
        AHO_NAMESPACE::ImageReadback reader(manager, 0, 2);
        VSL_NAMESPACE::ComputePhase phase(manager);
        phase << VSL_NAMESPACE::command::RenderPassBegin::WithClears(target, {
                                                                                 {color_id, VSL_NAMESPACE::ClearColor{1, 0, 0, 1}},
                                                                                 {object_id, VSL_NAMESPACE::ClearUint{77}},
                                                                             })
              << VSL_NAMESPACE::command::RenderPassEnd{};
        auto id_ticket = reader.record(phase, ids, VSL_NAMESPACE::ImageLayout::GENERAL, {1, 2, 1, 1}, 123);
        auto color_ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, 123);
        ASSERT_TRUE(id_ticket);
        ASSERT_TRUE(color_ticket);
        EXPECT_EQ(id_ticket->status(), VSL_NAMESPACE::Completion::Status::PENDING);
        EXPECT_FALSE(id_ticket->wait(0));
        EXPECT_THROW(id_ticket->read(0), std::logic_error);
        EXPECT_EQ(id_ticket->info().frame_id, 123u);
        EXPECT_EQ(id_ticket->info().region.x, 1u);
        EXPECT_EQ(id_ticket->info().region.y, 2u);
        EXPECT_EQ(id_ticket->info().row_stride, 4u);
        EXPECT_EQ(id_ticket->info().byte_size, 4u);
        EXPECT_EQ(color_ticket->info().region.width, WIDTH);
        EXPECT_GT(color_ticket->info().request_id, id_ticket->info().request_id);
        EXPECT_EQ(reader.capacity(), WIDTH * HEIGHT * 4u + 4u);
        ASSERT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::SUCCESS);
        auto selected = id_ticket->read();
        std::uint32_t id = 0;
        std::memcpy(&id, selected.bytes.data(), sizeof(id));
        EXPECT_EQ(id, 77u);
        EXPECT_EQ(color_ticket->read().bytes[0], std::byte{255});
        EXPECT_THROW(reader.record(phase, ids, VSL_NAMESPACE::ImageLayout::GENERAL), std::logic_error);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, CompletedTicketsRetainIndependentResultsAndOccupyTheirSlots) {
        auto manager = make_manager();
        AHO_NAMESPACE::ImageReadback reader(manager, 0, 2);
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto red = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, 7);
        ASSERT_TRUE(red);
        ASSERT_TRUE(red->wait());
        const auto first_id = red->info().request_id;
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 0, 1, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        auto blue = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, 8);
        ASSERT_TRUE(blue);
        ASSERT_TRUE(blue->wait());
        EXPECT_FALSE(reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL));
        EXPECT_EQ(red->read().bytes[0], std::byte{255});
        EXPECT_EQ(red->read().bytes[2], std::byte{0});
        EXPECT_EQ(blue->read().bytes[0], std::byte{0});
        EXPECT_EQ(blue->read().bytes[2], std::byte{255});
        EXPECT_EQ(red->info().frame_id, 7u);
        EXPECT_EQ(blue->info().frame_id, 8u);
        red.reset();
        auto next = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {0, 0, 1, 1}, 9);
        ASSERT_TRUE(next);
        EXPECT_GT(next->info().request_id, first_id);
        EXPECT_EQ(next->read().bytes[2], std::byte{255});
        EXPECT_EQ(blue->info().frame_id, 8u);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, TicketRemainsReadableAfterWorkerAndProducerAreReleased) {
        std::optional<AHO_NAMESPACE::ImageReadbackTicket> ticket;
        {
            auto manager = make_manager();
            AHO_NAMESPACE::ImageReadback reader(manager);
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 1, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
            ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, 42);
            ASSERT_TRUE(ticket);
            phase.submit();
        }
        ASSERT_TRUE(ticket->wait());
        EXPECT_EQ(ticket->info().frame_id, 42u);
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
                ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
                ASSERT_TRUE(ticket);
                if (attempt == 0)
                    phase.cancel();
                else
                    throw std::runtime_error("cancel recording");
            } catch (const std::runtime_error&) {
            }
            EXPECT_EQ(ticket->status(), VSL_NAMESPACE::Completion::Status::CANCELLED);
            EXPECT_FALSE(ticket->wait(0));
            EXPECT_THROW(ticket->read(0), std::logic_error);
            ticket.reset();
        }
        VSL_NAMESPACE::ComputePhase phase(manager);
        phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{0, 0, 1, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::GENERAL);
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
            auto ticket = reader.record(phase, *color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, frame);
            ASSERT_TRUE(ticket);
            phase.submit();
            ticket->read(pixels);
            EXPECT_EQ(ticket->info().frame_id, frame);
            EXPECT_EQ(pixels.bytes[0], std::byte{255});
            EXPECT_EQ(reader.capacity(), WIDTH * HEIGHT * 4u);
            if (storage)
                EXPECT_EQ(pixels.bytes.data(), storage);
            storage = pixels.bytes.data();
        }
        EXPECT_THROW((AHO_NAMESPACE::ImageReadback(manager, 0, 0)), std::invalid_argument);
    }
    TEST_F(ImageReadbackBusyVulkanIntegrationTest, MultipleTicketSlotsSubmitWithoutWaitingAndRejectExcessRequests) {
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<VSL_NAMESPACE::ComputePhase>();
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}})
                  << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        device.data_->device.waitIdle();
        AHO_NAMESPACE::ImageReadback reader(manager, 256, 2);
        vk::SemaphoreTypeCreateInfo type{.semaphoreType = vk::SemaphoreType::eTimeline};
        auto semaphore = device.data_->device.createSemaphoreUnique({.pNext = &type});
        const auto signal = reinterpret_cast<PFN_vkSignalSemaphoreKHR>(
            vkGetDeviceProcAddr(device.data_->device, "vkSignalSemaphoreKHR"));
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
        } unblock{device.data_->device, *semaphore, signal};
        const std::uint64_t value = 1;
        vk::TimelineSemaphoreSubmitInfo timeline{.waitSemaphoreValueCount = 1, .pWaitSemaphoreValues = &value};
        const vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eAllCommands;
        manager.data_->graphics_queue.submit(vk::SubmitInfo{
            .pNext = &timeline, .waitSemaphoreCount = 1, .pWaitSemaphores = &*semaphore, .pWaitDstStageMask = &stage});
        auto first = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, 10);
        auto second = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {0, 0, 1, 1}, 11);
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_FALSE(first->ready());
        EXPECT_FALSE(second->ready());
        EXPECT_FALSE(first->wait(0));
        EXPECT_FALSE(second->wait(0));
        EXPECT_FALSE(reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {}, 12));
        EXPECT_EQ(first->info().frame_id, 10u);
        EXPECT_EQ(second->info().frame_id, 11u);
        unblock.release();
        auto pixels = first->read();
        EXPECT_EQ(pixels.width, 8u);
        EXPECT_EQ(pixels.height, 8u);
        EXPECT_EQ(pixels.bytes.size(), 256u);
        EXPECT_EQ(pixels.bytes[0], std::byte{255});
        EXPECT_EQ(second->read().bytes.size(), 4u);
        EXPECT_EQ(second->info().frame_id, 11u);
    }
    TEST_F(ImageReadbackVulkanIntegrationTest, DiscardLatestSwitchesFromConvenienceCaptureToTicketWithoutRebuildingWorker) {
        auto manager = make_manager();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer, {VSL_NAMESPACE::ClearColor{1, 0, 0, 1}}) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback reader(manager);
        ASSERT_TRUE(reader.capture(*color, VSL_NAMESPACE::ImageLayout::GENERAL));
        ASSERT_TRUE(reader.wait());
        EXPECT_FALSE(reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL));
        reader.discard_latest();
        EXPECT_FALSE(reader.ready());
        EXPECT_THROW(reader.read(), std::logic_error);
        auto ticket = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL);
        ASSERT_TRUE(ticket);
        EXPECT_EQ(ticket->read().bytes[0], std::byte{255});
        EXPECT_EQ(reader.capacity(), WIDTH * HEIGHT * 4u);
    }

    TEST_F(ImageReadbackVulkanIntegrationTest, UsesAnAvailableLargeSlotBeforeGrowingAnotherSlot) {
        auto manager = make_manager();
        {
            VSL_NAMESPACE::ComputePhase phase(manager);
            phase << VSL_NAMESPACE::command::RenderPassBegin(*framebuffer) << VSL_NAMESPACE::command::RenderPassEnd{};
        }
        AHO_NAMESPACE::ImageReadback reader(manager, 4, 2);
        auto small = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL, {0, 0, 1, 1});
        ASSERT_TRUE(small);
        ASSERT_TRUE(small->wait());
        auto large = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL);
        ASSERT_TRUE(large);
        ASSERT_TRUE(large->wait());
        EXPECT_EQ(reader.capacity(), 4u + WIDTH * HEIGHT * 4u);
        small.reset();
        large.reset();
        auto next = reader.capture_ticket(*color, VSL_NAMESPACE::ImageLayout::GENERAL);
        ASSERT_TRUE(next);
        EXPECT_TRUE(next->wait());
        EXPECT_EQ(reader.capacity(), 4u + WIDTH * HEIGHT * 4u);
    }

} // namespace
#endif
