#pragma once
#include "vulkan_fixture.hpp"
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>
#include <array>
#include <functional>
#include <optional>

namespace vsl_test {

    class RenderVulkanIntegrationTest : public VulkanIntegrationTest {
    protected:
        static constexpr std::uint32_t WIDTH = 8, HEIGHT = 8;
        using ColorImage = vsl::Image<vsl::ImageType::COLOR_ATTACHMENT | vsl::ImageType::TRANSFER_SRC>;
        std::optional<ColorImage> color;
        vsl::SwapchainAccessor description;
        std::optional<vsl::RenderPass> pass;
        std::optional<vsl::FrameBuffer> framebuffer;
        std::optional<vsl::Shader<vsl::ShaderType::VERTEX>> vertex;
        std::optional<vsl::Shader<vsl::ShaderType::FRAGMENT>> fragment;
        std::optional<vsl::PipelineLayout> layout;
        std::optional<vsl::GraphicsPipeline> pipeline;

        void SetUp() override;
        void configure_pipeline(bool with_vertex_inputs);
        std::array<std::uint8_t, 4> render(const std::function<void(vsl::DefaultPhaseStream&)>& commands);
        static void ExpectTriangleColor(const std::array<std::uint8_t, 4>& pixel);
        void TearDown() override;
    };

    inline void RenderVulkanIntegrationTest::SetUp() {
        VulkanIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        if (!host_coherent_available())
            GTEST_SKIP() << "Host coherent memory unavailable";
        color.emplace(device, WIDTH, HEIGHT, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        description.data_ = std::make_shared<vsl::impl::SwapchainImpl>();
        description.data_->device = device.data_;
        description.data_->extent = description.data_->swap_chain_extent = vk::Extent2D{WIDTH, HEIGHT};
        description.data_->image_count = 1;
        description.data_->swap_chain_image_format = vk::Format::eR8G8B8A8Unorm;
        description.images = {*color};
        vsl::RenderPassDescription pass_description;
        pass_description.add_color({.format = vsl::data_format::UNSIGNED_NORMALIZED8_RGBA,
                                    .final_layout = vsl::ImageLayout::GENERAL});
        pass.emplace(device, pass_description);
        framebuffer.emplace(*pass, WIDTH, HEIGHT, std::vector<vsl::ImageAccessor>{*color});
        configure_pipeline(false);
    }

    inline void RenderVulkanIntegrationTest::configure_pipeline(bool with_vertex_inputs) {
        pipeline.reset();
        layout.reset();
        vertex.emplace(device, shader_path(with_vertex_inputs ? "vertices.vert.spv" : "triangle.vert.spv"));
        fragment.emplace(device, shader_path("triangle.frag.spv"));
        namespace pl = vsl::pipeline_layout;
        pl::VertexInput input;
        if (with_vertex_inputs)
            input.add({vsl::data_format::FLOAT_RG, vsl::data_format::FLOAT_RGB});
        layout.emplace(device, pl::ShaderGroup("offscreen", {*vertex, *fragment}), input, pl::ColorBlend{},
                       pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::NONE}, pl::Multisample{},
                       pl::DynamicState{}, vsl::Viewport(description), vsl::Scissor(description));
        pipeline.emplace(*layout, *pass);
    }

    inline std::array<std::uint8_t, 4> RenderVulkanIntegrationTest::render(const std::function<void(vsl::DefaultPhaseStream&)>& commands) {
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto stream = phase << vsl::command::RenderPassBegin(*pass, *framebuffer);
            stream << *pipeline << vsl::Viewport(description) << vsl::Scissor(description);
            commands(stream);
            stream << vsl::command::RenderPassEnd{};
        }
        device.data_->device.waitIdle();
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> readback(device, WIDTH * HEIGHT * 4);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::CopyImageToBuffer(&readback, *color);
        }
        device.data_->device.waitIdle();
        auto mapped = readback.data(WIDTH * HEIGHT * 4);
        std::array<std::uint8_t, 4> result{};
        // 中央ピクセルを読み、端のラスタライズ規則から独立した検証にする。
        std::memcpy(result.data(),
                    static_cast<const char*>(mapped.data) + ((HEIGHT / 2) * WIDTH + WIDTH / 2) * 4, 4);
        return result;
    }

    inline void RenderVulkanIntegrationTest::ExpectTriangleColor(const std::array<std::uint8_t, 4>& pixel) {
        // UNORM8 変換の丸め差を1段階だけ許容する。
        EXPECT_NEAR(static_cast<int>(pixel[0]), 64, 1);
        EXPECT_NEAR(static_cast<int>(pixel[1]), 128, 1);
        EXPECT_NEAR(static_cast<int>(pixel[2]), 191, 1);
        EXPECT_EQ(pixel[3], 255);
    }

    inline void RenderVulkanIntegrationTest::TearDown() {
        if (device.data_)
            device.data_->device.waitIdle();
        pipeline.reset();
        layout.reset();
        vertex.reset();
        fragment.reset();
        framebuffer.reset();
        pass.reset();
        description.images.clear();
        description.data_.reset();
        color.reset();
        VulkanIntegrationTest::TearDown();
    }

} // namespace vsl_test
