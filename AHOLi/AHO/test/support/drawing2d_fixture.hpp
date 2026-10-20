#pragma once
#include "support/render_fixture.hpp"
#include <AHO/image_readback.hpp>
#include <AHO/x/drawer/renderer2d.hpp>
#include <VSL/vulkan/commands/copy_buffer_to_image.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>

namespace aho_test {
    class Drawing2DVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;

    protected:
        using Renderer = aho::x::Renderer2D;
        using List = aho::x::DrawList;
        using Texture = vsl::Image<vsl::ImageType::Sampled | vsl::ImageType::TransferDst>;
        static aho::RGBA Red();
        static aho::RGBA Green();
        aho::ImagePixels pixels(vsl::CommandManager manager);
        Texture texture(const std::array<std::uint8_t, 16>& bytes);
        static std::array<unsigned, 4> Pixel(const aho::ImagePixels& image, std::size_t x, std::size_t y);
    };

    inline bool Drawing2DVulkanIntegrationTest::validate_vulkan() const { return true; }

    inline aho::RGBA Drawing2DVulkanIntegrationTest::Red() { return {1, 0, 0, 1}; }

    inline aho::RGBA Drawing2DVulkanIntegrationTest::Green() { return {0, 1, 0, 1}; }

    inline aho::ImagePixels Drawing2DVulkanIntegrationTest::pixels(vsl::CommandManager manager) {
        aho::ImageReadback readback(manager);
        auto ticket = readback.captureTicket(*color, vsl::ImageLayout::General);
        if (!ticket)
            throw std::runtime_error("No readback slot");
        return ticket->read();
    }

    inline std::array<unsigned, 4> Drawing2DVulkanIntegrationTest::Pixel(const aho::ImagePixels& image, std::size_t x, std::size_t y) {
        const auto i = y * image.rowStride + x * 4;
        return {std::to_integer<unsigned>(image.bytes[i]), std::to_integer<unsigned>(image.bytes[i + 1]),
                std::to_integer<unsigned>(image.bytes[i + 2]), std::to_integer<unsigned>(image.bytes[i + 3])};
    }

    inline Drawing2DVulkanIntegrationTest::Texture Drawing2DVulkanIntegrationTest::texture(const std::array<std::uint8_t, 16>& bytes) {
        Texture image(device, 2, 2, vsl::data_format::UnsignedNormalized8RGBA);
        vsl::StagingBuffer<> staging(device, bytes.size());
        {
            auto mapped = staging.data(bytes.size());
            std::memcpy(mapped.data, bytes.data(), bytes.size());
        }
        auto manager = make_manager();
        auto phase = manager.startPhase<vsl::ComputePhase>();
        phase << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::Undefined, vsl::ImageLayout::TransferDstOptimal)
              << vsl::command::CopyBufferToImage(image, &staging, vsl::ImageLayout::TransferDstOptimal)
              << vsl::command::ChangeImageBarrier(image, vsl::ImageLayout::TransferDstOptimal, vsl::ImageLayout::ShaderReadOnlyOptimal);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::Success || !phase.completion().wait())
            throw std::runtime_error("Texture upload failed");
        return image;
    }
} // namespace aho_test
