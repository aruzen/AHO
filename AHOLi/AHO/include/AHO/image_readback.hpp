#pragma once
#include "define.hpp"
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/image.hpp>
#include <cstddef>
#include <memory>
#include <vector>

namespace AHO_NAMESPACE {
    struct ImagePixels {
        std::uint32_t width = 0, height = 0;
        std::size_t rowStride = 0;
        vsl::data_format::___Format format = vsl::data_format::Undefined;
        // Tightly packed native texels. Encoding and color conversion belong to the caller.
        std::vector<std::byte> bytes;
    };

    // Reusable worker with one outstanding transfer. Copies share the same worker.
    class ImageReadback {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit ImageReadback(vsl::CommandManager manager, std::size_t initialCapacity = 0);
        // Convenience for a first capture. Keep this object and use capture() for subsequent frames.
        ImageReadback(vsl::CommandManager manager, vsl::ImageAccessor image, vsl::ImageLayout currentLayout,
                      vsl::ImageRegion region = {});

        // Submit after the producer on this manager's graphics queue. False means the previous copy is busy.
        // Accepted captures replace the previous result. Buffer capacity only grows when needed.
        bool capture(vsl::ImageAccessor image, vsl::ImageLayout currentLayout, vsl::ImageRegion region = {});
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        // Reuse the caller's CPU vector capacity as well.
        void read(ImagePixels &destination, std::uint64_t timeout_ns = UINT64_MAX) const;
        std::size_t capacity() const;
    };
}
