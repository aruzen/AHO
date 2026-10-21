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
#include <stb_image_write.h>

namespace AHO_NAMESPACE::x {
    namespace {
        bool isBgra(VSL_NAMESPACE::data_format::___Format format) {
            using enum VSL_NAMESPACE::data_format::___FormatTypes;
            switch (format.type()) {
                case Srgb8RGBA:
                case UnsignedNormalized8RGBA:
                    return false;
                case Srgb8BGRA:
                case UnsignedNormalized8BGRA:
                    return true;
                default:
                    throw std::invalid_argument("Screenshot requires RGBA8 or BGRA8 pixels");
            }
        }
    } // namespace

    void saveScreenshot(const ImagePixels& pixels, const std::filesystem::path& path) {
        const bool bgra = isBgra(pixels.format);
        // エンコーダー内部の符号付き整数演算と作業領域にも余裕を確保する。
        constexpr auto limit = std::size_t(std::numeric_limits<int>::max()) / 2;
        if (!pixels.width || !pixels.height || pixels.width > (limit - 1) / 4)
            throw std::invalid_argument("Invalid screenshot dimensions");
        const auto rowBytes = std::size_t(pixels.width) * 4;
        if (pixels.height > limit / (rowBytes + 1))
            throw std::invalid_argument("Screenshot is too large to encode");
        if (pixels.rowStride < rowBytes || pixels.rowStride > std::size_t(std::numeric_limits<int>::max()) ||
            pixels.bytes.size() < rowBytes ||
            pixels.height - 1 > (pixels.bytes.size() - rowBytes) / pixels.rowStride)
            throw std::invalid_argument("Screenshot pixel storage is too small");
        std::vector<std::byte> converted;
        auto* source = reinterpret_cast<const unsigned char*>(pixels.bytes.data());
        auto stride = pixels.rowStride;
        if (bgra) {
            converted.resize(rowBytes * pixels.height);
            for (std::size_t y = 0; y < pixels.height; ++y) {
                const auto* input = pixels.bytes.data() + y * pixels.rowStride;
                auto* output = converted.data() + y * rowBytes;
                for (std::size_t x = 0; x < rowBytes; x += 4) {
                    output[x] = input[x + 2];
                    output[x + 1] = input[x + 1];
                    output[x + 2] = input[x];
                    output[x + 3] = input[x + 3];
                }
            }
            source = reinterpret_cast<const unsigned char*>(converted.data());
            stride = rowBytes;
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

    struct Screenshot::Data {
        std::weak_ptr<window::WindowData> window;
        ImageReadback reader;
        std::optional<ImageReadbackTicket> ticket;
        VSL_NAMESPACE::Completion completion;

        Data(std::shared_ptr<window::WindowData> window, VSL_NAMESPACE::CommandManager manager)
            : window(std::move(window)), reader(std::move(manager)) {}
    };

    Screenshot::Screenshot(window::Window window) {
        if (!window || !window._data2 || !window.engine._data)
            throw std::invalid_argument("Screenshot requires a live AHO window");
        if (THREAD_LOCAL_DRAW_PHASE)
            throw std::logic_error("Create Screenshot outside a DrawPhase");
        auto& resources = *window._data2;
        auto manager = window.engine._data->command_manager;
        (void)isBgra(resources.swapchain.getFormat());
        auto data = std::make_shared<Data>(window._data2, manager);
        const auto usage = resources.swapchain.images.at(0).getUsage();
        if (!VSL_NAMESPACE::contain(usage, VSL_NAMESPACE::ImageType::TransferSrc)) {
            manager.waitIdle();
            auto swapchain = VSL_NAMESPACE::Swapchain(manager.getDevice(), resources.surface, resources.swapchain,
                                                      std::nullopt, std::nullopt, usage | VSL_NAMESPACE::ImageType::TransferSrc);
            auto target = VSL_NAMESPACE::FrameBuffer(swapchain, resources.render_pass);
            if (resources.render_finished.size() != swapchain.getSwapImageSize())
                resources.render_finished = window.engine._data->synchro_manager.createSemaphore(
                    window.name() + "ScreenshotFinished", swapchain.getSwapImageSize());
            resources.swapchain = std::move(swapchain);
            resources.frame_buffer = std::move(target);
        }
        _data = std::move(data);
    }

    bool Screenshot::capture(DrawPhase& phase) {
        if (!_data || phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit)
            throw std::logic_error("Screenshot requires a recording DrawPhase");
        const auto window = _data->window.lock();
        if (!window || !phase.engine || phase.engine->getWindow()._data2 != window ||
            phase.swapchain._data != window->swapchain._data)
            throw std::invalid_argument("DrawPhase belongs to a different or closed window");
        const auto image = phase.swapchain.images.at(phase.getImageIndex());
        if (!VSL_NAMESPACE::contain(image.getUsage(), VSL_NAMESPACE::ImageType::TransferSrc))
            throw std::logic_error("Window no longer supports screenshot readback");
        const auto status = _data->completion.status();
        if (status == VSL_NAMESPACE::Completion::Status::Pending || status == VSL_NAMESPACE::Completion::Status::Submitted)
            return false;
        auto finish = [previous = phase.finish, state = _data, &phase, image](auto& stream) {
            if (previous)
                previous(stream);
            else
                stream << VSL_NAMESPACE::command::RenderPassEnd{};
            state->ticket = state->reader.record(phase, image, VSL_NAMESPACE::ImageLayout::Present);
            if (!state->ticket)
                throw std::logic_error("Screenshot readback slot is unavailable");
        };
        phase.finish = std::move(finish);
        _data->ticket.reset();
        _data->completion = phase.completion();
        return true;
    }

    bool Screenshot::ready() const { return _data && _data->ticket && _data->ticket->ready(); }

    ImagePixels Screenshot::read(std::uint64_t timeout) const {
        if (!_data || !_data->ticket)
            throw std::logic_error("Screenshot has not been submitted or was cancelled");
        return _data->ticket->read(timeout);
    }

    void Screenshot::save(const std::filesystem::path& path, std::uint64_t timeout) const {
        saveScreenshot(read(timeout), path);
    }
} // namespace AHO_NAMESPACE::x
