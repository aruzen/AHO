//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_COPY_IMAGE_TO_BUFFER_H
#define AHO_ALL_COPY_IMAGE_TO_BUFFER_H

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

#include "../image.hpp"
#include "../buffer.hpp"

namespace VSL_NAMESPACE::command {
    struct CopyImageToBuffer : public __Command {
        CopyImageToBuffer(BufferAccessor *buffer, ImageAccessor image);

        template<MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode,
                vsl::ImageType ImgType, vsl::MemoryProperty ImgMemProp, vsl::ImageLayout Layout>
        CopyImageToBuffer(Buffer<MemType, BufMemProp, ShareMode> &buffer, Image<ImgType, ImgMemProp, Layout> &image)
                : CopyImageToBuffer(&buffer, image) {};

        ImageAccessor image;
        BufferAccessor *buffer;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}

#endif //AHO_ALL_COPY_IMAGE_TO_BUFFER_H
