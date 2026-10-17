#pragma once
#include "command.hpp"
#include "image.hpp"
#include <cstddef>
#include <memory>
#include <vector>

namespace VSL_NAMESPACE {
    struct ImagePixels {
        std::uint32_t width = 0, height = 0;
        std::size_t rowStride = 0;
        data_format::___Format format = data_format::Undefined;
        // Tightly packed native texels: format/order/color space are preserved.
        std::vector<std::byte> bytes;
    };
    class ImageReadback {
        std::shared_ptr<_impl::ImageReadback_impl> data_;

    public:
        // Submit after the producer on this manager's graphics queue, outside a render pass.
        // currentLayout is the actual layout. It is restored by the submitted copy.
        // Source must have TransferSrc usage. Cross-queue synchronization is caller-owned.
        ImageReadback(CommandManager manager, ImageAccessor image, ImageLayout currentLayout,
                      ImageRegion region = {});
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
    };
} // namespace VSL_NAMESPACE
