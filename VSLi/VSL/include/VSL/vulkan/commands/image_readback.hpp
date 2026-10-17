#pragma once
#include "copy_image_to_buffer.hpp"

namespace VSL_NAMESPACE::command {
    // Records layout transition, image-to-buffer copy, host visibility, and layout restoration.
    // Invoke outside a render pass. Submission, completion, and resource lifetime belong to the caller.
    struct ImageReadback : public __Command {
        ImageReadback(BufferAccessor *destination, ImageAccessor image, ImageLayout currentLayout,
                      ImageRegion region = {});
        ImageReadback(std::shared_ptr<_impl::Buffer_impl> destination, ImageAccessor image,
                      ImageLayout currentLayout, ImageRegion region = {});

        CopyImageToBuffer copy;
        ImageLayout currentLayout;

        static ImageCopyInfo describe(ImageAccessor image, ImageLayout currentLayout, ImageRegion region = {});
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
}
