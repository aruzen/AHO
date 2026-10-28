#include <AHO/x/screenshot.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <AHO/window.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../thirdparty/stb_image_write.h"

namespace AHO_NAMESPACE::x {
    struct Screenshot::Data {
        std::weak_ptr<window::WindowData> window;
        ImageReadback reader;
        std::optional<ImageReadbackTicket> ticket;
        VSL_NAMESPACE::Completion completion;

        Data(std::shared_ptr<window::WindowData> window, VSL_NAMESPACE::CommandManager manager);
    };

    namespace {
        bool is_bgra(VSL_NAMESPACE::data_format::Format format);
    } // namespace

    Screenshot::Data::Data(std::shared_ptr<window::WindowData> window, VSL_NAMESPACE::CommandManager manager)
        : window(std::move(window)), reader(std::move(manager)) {}

    namespace {
        bool is_bgra(VSL_NAMESPACE::data_format::Format format) {
            using enum VSL_NAMESPACE::data_format::FormatTypes;
            switch (format.type()) {
                case SRGB8_RGBA:
                case UNSIGNED_NORMALIZED8_RGBA:
                    return false;
                case SRGB8_BGRA:
                case UNSIGNED_NORMALIZED8_BGRA:
                    return true;
                default:
                    throw std::invalid_argument("Screenshot requires RGBA8 or BGRA8 pixels");
            }
        }
    } // namespace

    void save_screenshot(const ImagePixels& pixels, const std::filesystem::path& path) {
        const bool bgra = is_bgra(pixels.format);
        // エンコーダー内部の符号付き整数演算と作業領域にも余裕を確保する。
        constexpr auto limit = std::size_t(std::numeric_limits<int>::max()) / 2;
        if (!pixels.width || !pixels.height || pixels.width > (limit - 1) / 4)
            throw std::invalid_argument("Invalid screenshot dimensions");
        const auto row_bytes = std::size_t(pixels.width) * 4;
        if (pixels.height > limit / (row_bytes + 1))
            throw std::invalid_argument("Screenshot is too large to encode");
        if (pixels.row_stride < row_bytes || pixels.row_stride > std::size_t(std::numeric_limits<int>::max()) ||
            pixels.bytes.size() < row_bytes ||
            pixels.height - 1 > (pixels.bytes.size() - row_bytes) / pixels.row_stride)
            throw std::invalid_argument("Screenshot pixel storage is too small");
        std::vector<std::byte> converted;
        auto* source = reinterpret_cast<const unsigned char*>(pixels.bytes.data());
        auto stride = pixels.row_stride;
        if (bgra) {
            converted.resize(row_bytes * pixels.height);
            for (std::size_t y = 0; y < pixels.height; ++y) {
                const auto* input = pixels.bytes.data() + y * pixels.row_stride;
                auto* output = converted.data() + y * row_bytes;
                for (std::size_t x = 0; x < row_bytes; x += 4) {
                    output[x] = input[x + 2];
                    output[x + 1] = input[x + 1];
                    output[x + 2] = input[x];
                    output[x + 3] = input[x + 3];
                }
            }
            source = reinterpret_cast<const unsigned char*>(converted.data());
            stride = row_bytes;
        }
        int size = 0;
        std::unique_ptr<unsigned char, decltype(&std::free)> encoded(
            stbi_write_png_to_mem(source, static_cast<int>(stride), static_cast<int>(pixels.width),
                                  static_cast<int>(pixels.height), 4, &size),
            &std::free);
        if (!encoded)
            throw std::runtime_error("Could not encode screenshot as PNG");
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(encoded.get()), size);
        output.close();
        if (!output)
            throw std::runtime_error("Could not write screenshot: " + path.string());
    }

    Screenshot::Screenshot(window::Window window) {
        if (!window || !window.data2 || !window.engine.data())
            throw std::invalid_argument("Screenshot requires a live AHO window");
        if (THREAD_LOCAL_DRAW_PHASE)
            throw std::logic_error("Create Screenshot outside a DrawPhase");
        auto& resources = *window.data2;
        auto manager = window.engine.data()->command_manager;
        (void)is_bgra(resources.swapchain.get_format());
        auto data = std::make_shared<Data>(window.data2, manager);
        const auto usage = resources.swapchain.images.at(0).get_usage();
        if (!VSL_NAMESPACE::contain(usage, VSL_NAMESPACE::ImageType::TRANSFER_SRC))
            window.recreate_swapchain(VSL_NAMESPACE::ImageType::TRANSFER_SRC);
        _data = std::move(data);
    }

    bool Screenshot::capture(DrawPhase& phase) {
        if (!_data || phase.state != VSL_NAMESPACE::ComputePhase::State::WAIT_SUBMIT)
            throw std::logic_error("Screenshot requires a recording DrawPhase");
        const auto window = _data->window.lock();
        if (!window || !phase.engine || phase.engine->get_window().data2 != window ||
            phase.swapchain.data_ != window->swapchain.data_)
            throw std::invalid_argument("DrawPhase belongs to a different or closed window");
        const auto image = phase.swapchain.images.at(phase.get_image_index());
        if (!VSL_NAMESPACE::contain(image.get_usage(), VSL_NAMESPACE::ImageType::TRANSFER_SRC))
            throw std::logic_error("Window no longer supports screenshot readback");
        const auto status = _data->completion.status();
        if (status == VSL_NAMESPACE::Completion::Status::PENDING || status == VSL_NAMESPACE::Completion::Status::SUBMITTED)
            return false;
        auto finish = [previous = phase.finish, state = _data, &phase, image](auto& stream) {
            if (previous)
                previous(stream);
            else
                stream << VSL_NAMESPACE::command::RenderPassEnd{};
            state->ticket = state->reader.record(phase, image, VSL_NAMESPACE::ImageLayout::PRESENT);
            if (!state->ticket)
                throw std::logic_error("Screenshot readback slot is unavailable");
        };
        phase.finish = std::move(finish);
        _data->ticket.reset();
        _data->completion = phase.completion();
        return true;
    }

    bool Screenshot::ready() const {
        return _data && _data->ticket && _data->ticket->ready();
    }

    ImagePixels Screenshot::read(std::uint64_t timeout) const {
        if (!_data || !_data->ticket)
            throw std::logic_error("Screenshot has not been submitted or was cancelled");
        return _data->ticket->read(timeout);
    }

    void Screenshot::save(const std::filesystem::path& path, std::uint64_t timeout) const {
        save_screenshot(read(timeout), path);
    }
} // namespace AHO_NAMESPACE::x
