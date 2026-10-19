#pragma once
#include "vulkan_fixture.hpp"
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>
#include <array>
#include <functional>
#include <optional>

namespace vsl_test {

    class RenderVulkanIntegrationTest : public VulkanIntegrationTest {
    protected:
        static constexpr std::uint32_t width = 8, height = 8;
        using ColorImage = vsl::Image<vsl::ImageType::ColorAttachment | vsl::ImageType::TransferSrc>;
        std::optional<ColorImage> color;
        vsl::SwapchainAccessor description;
        std::optional<vsl::RenderPass> pass;
        std::optional<vsl::FrameBuffer> framebuffer;
        std::optional<vsl::Shader<vsl::ShaderType::Vertex>> vertex;
        std::optional<vsl::Shader<vsl::ShaderType::Fragment>> fragment;
        std::optional<vsl::PipelineLayout> layout;
        std::optional<vsl::GraphicsPipeline> pipeline;

        void SetUp() override {
            VulkanIntegrationTest::SetUp();
            if (HasFatalFailure() || IsSkipped())
                return;
            if (!host_coherent_available())
                GTEST_SKIP() << "Host coherent memory unavailable";
            color.emplace(device, width, height, vsl::data_format::UnsignedNormalized8RGBA);
            description._data = std::make_shared<vsl::_impl::Swapchain_impl>();
            description._data->device = device._data;
            description._data->extent = description._data->swapChainExtent = vk::Extent2D{width, height};
            description._data->imageCount = 1;
            description._data->swapChainImageFormat = vk::Format::eR8G8B8A8Unorm;
            description.images = {*color};
            vsl::RenderPassDescription passDescription;
            passDescription.addColor({.format = vsl::data_format::UnsignedNormalized8RGBA,
                                      .finalLayout = vsl::ImageLayout::General});
            pass.emplace(device, passDescription);
            framebuffer.emplace(*pass, width, height, std::vector<vsl::ImageAccessor>{*color});
            configure_pipeline(false);
        }

        void configure_pipeline(bool with_vertex_inputs) {
            pipeline.reset();
            layout.reset();
            vertex.emplace(device, shader_path(with_vertex_inputs ? "vertices.vert.spv" : "triangle.vert.spv"));
            fragment.emplace(device, shader_path("triangle.frag.spv"));
            namespace pl = vsl::pipeline_layout;
            pl::VertexInput input;
            if (with_vertex_inputs)
                input.add({vsl::data_format::FloatRG, vsl::data_format::FloatRGB});
            layout.emplace(device, pl::ShaderGroup("offscreen", {*vertex, *fragment}), input, pl::ColorBlend{},
                           pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None}, pl::Multisample{},
                           pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
            pipeline.emplace(*layout, *pass);
        }

        std::array<std::uint8_t, 4> render(const std::function<void(vsl::DefaultPhaseStream&)>& commands) {
            auto manager = make_manager();
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                auto stream = phase << vsl::command::RenderPassBegin(*pass, *framebuffer);
                stream << *pipeline << vsl::Viewport(description) << vsl::Scissor(description);
                commands(stream);
                stream << vsl::command::RenderPassEnd{};
            }
            device._data->device.waitIdle();
            vsl::StagingBuffer<vsl::MemoryType::TransferDestination> readback(device, width * height * 4);
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                phase << vsl::command::CopyImageToBuffer(&readback, *color);
            }
            device._data->device.waitIdle();
            auto mapped = readback.data(width * height * 4);
            std::array<std::uint8_t, 4> result{};
            // 中央ピクセルを読み、端のラスタライズ規則から独立した検証にする。
            std::memcpy(result.data(),
                        static_cast<const char*>(mapped.data) + ((height / 2) * width + width / 2) * 4, 4);
            return result;
        }

        static void expect_triangle_color(const std::array<std::uint8_t, 4>& pixel) {
            // UNORM8 変換の丸め差を1段階だけ許容する。
            EXPECT_NEAR(static_cast<int>(pixel[0]), 64, 1);
            EXPECT_NEAR(static_cast<int>(pixel[1]), 128, 1);
            EXPECT_NEAR(static_cast<int>(pixel[2]), 191, 1);
            EXPECT_EQ(pixel[3], 255);
        }

        void TearDown() override {
            if (device._data)
                device._data->device.waitIdle();
            pipeline.reset();
            layout.reset();
            vertex.reset();
            fragment.reset();
            framebuffer.reset();
            pass.reset();
            description.images.clear();
            description._data.reset();
            color.reset();
            VulkanIntegrationTest::TearDown();
        }
    };

} // namespace vsl_test
