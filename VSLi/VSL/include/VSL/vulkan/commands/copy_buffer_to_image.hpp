//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_COPY_BUFFER_TO_IMAGE_H
#define AHO_ALL_COPY_BUFFER_TO_IMAGE_H

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../pipeline.hpp"

#include "../image.hpp"
#include "../buffer.hpp"

namespace VSL_NAMESPACE::command {
    struct CopyBufferToImage : public __Command {
        CopyBufferToImage(ImageAccessor image, BufferAccessor *buffer, vsl::ImageLayout layout);

        template<vsl::ImageType ImgType, vsl::MemoryProperty ImgMemProp, vsl::ImageLayout Layout,
                MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode>
        CopyBufferToImage(Image<ImgType, ImgMemProp, Layout> &image, Buffer<MemType, BufMemProp, ShareMode> &buffer)
                : CopyBufferToImage(image, &buffer, Layout) {};

        vsl::ImageLayout layout;
        ImageAccessor image;
        BufferAccessor *buffer;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };

    struct CopyBufferToArrayImage : public __Command {
        CopyBufferToArrayImage(ImageAccessor image, BufferAccessor *buffer, vsl::ImageLayout layout, std::uint32_t target_idx);

        template<vsl::ImageType ImgType, vsl::MemoryProperty ImgMemProp, vsl::ImageLayout Layout,
                MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode>
        CopyBufferToArrayImage(Image<ImgType, ImgMemProp, Layout> &image, Buffer<MemType, BufMemProp, ShareMode> &buffer, std::uint32_t target_idx)
                : CopyBufferToImage(image, &buffer, Layout, target_idx) {};

        vsl::ImageLayout layout;
        ImageAccessor image;
        BufferAccessor *buffer{};
        std::uint32_t target_idx{};

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}

#endif //AHO_ALL_COPY_BUFFER_TO_IMAGE_H
