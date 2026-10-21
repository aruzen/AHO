#pragma once
#include "support/render_fixture.hpp"
#include <AHO/image_readback.hpp>
#include <AHO/x/drawer/renderer2d.hpp>

namespace aho_test {
    class Drawing2DVulkanIntegrationTest : public vsl_test::RenderVulkanIntegrationTest {
        bool validate_vulkan() const override;

    protected:
        using Renderer = aho::x::Renderer2D;
        using List = aho::x::DrawList;
        static aho::RGBA red();
        static aho::RGBA green();
        aho::ImagePixels pixels(vsl::CommandManager manager);
        static std::array<unsigned, 4> pixel(const aho::ImagePixels& image, std::size_t x, std::size_t y);
    };

    inline bool Drawing2DVulkanIntegrationTest::validate_vulkan() const { return true; }

    inline aho::RGBA Drawing2DVulkanIntegrationTest::red() { return {1, 0, 0, 1}; }

    inline aho::RGBA Drawing2DVulkanIntegrationTest::green() { return {0, 1, 0, 1}; }

    inline aho::ImagePixels Drawing2DVulkanIntegrationTest::pixels(vsl::CommandManager manager) {
        aho::ImageReadback readback(manager);
        auto ticket = readback.captureTicket(*color, vsl::ImageLayout::General);
        if (!ticket)
            throw std::runtime_error("No readback slot");
        return ticket->read();
    }

    inline std::array<unsigned, 4> Drawing2DVulkanIntegrationTest::pixel(const aho::ImagePixels& image, std::size_t x, std::size_t y) {
        const auto i = y * image.rowStride + x * 4;
        return {std::to_integer<unsigned>(image.bytes[i]), std::to_integer<unsigned>(image.bytes[i + 1]),
                std::to_integer<unsigned>(image.bytes[i + 2]), std::to_integer<unsigned>(image.bytes[i + 3])};
    }
} // namespace aho_test
