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
#include "copy_image_to_buffer.hpp"

namespace VSL_NAMESPACE::command {
    struct CopyBufferToImage : public __Command {
        CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer, ImageLayout layout);
        CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer, ImageLayout layout, ImageRegion region, std::size_t offset = 0);

        template<VSL_NAMESPACE::ImageType ImgType, VSL_NAMESPACE::MemoryProperty ImgMemProp, VSL_NAMESPACE::ImageLayout Layout,
                 MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode>
        CopyBufferToImage(Image<ImgType, ImgMemProp, Layout>& image, Buffer<MemType, BufMemProp, ShareMode>& buffer);

        VSL_NAMESPACE::ImageLayout layout;
        ImageAccessor image;
        BufferAccessor* buffer;
        ImageRegion region;
        std::size_t offset = 0;

        ImageCopyInfo description() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;

    private:
        std::shared_ptr<_impl::Buffer_impl> source_;
    };

    struct CopyBufferToArrayImage : public __Command {
        CopyBufferToArrayImage(ImageAccessor image, BufferAccessor* buffer, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx);

        template<VSL_NAMESPACE::ImageType ImgType, VSL_NAMESPACE::MemoryProperty ImgMemProp, VSL_NAMESPACE::ImageLayout Layout,
                 MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode>
        CopyBufferToArrayImage(Image<ImgType, ImgMemProp, Layout>& image, Buffer<MemType, BufMemProp, ShareMode>& buffer, std::uint32_t target_idx);

        VSL_NAMESPACE::ImageLayout layout;
        ImageAccessor image;
        BufferAccessor* buffer{};
        std::uint32_t target_idx{};

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::shared_ptr<_impl::Buffer_impl> source_;
    };

    template<VSL_NAMESPACE::ImageType ImgType, VSL_NAMESPACE::MemoryProperty ImgMemProp, VSL_NAMESPACE::ImageLayout Layout,
             MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode>
    CopyBufferToImage::CopyBufferToImage(Image<ImgType, ImgMemProp, Layout>& image, Buffer<MemType, BufMemProp, ShareMode>& buffer)
        : CopyBufferToImage(image, &buffer, Layout) {}

    template<VSL_NAMESPACE::ImageType ImgType, VSL_NAMESPACE::MemoryProperty ImgMemProp, VSL_NAMESPACE::ImageLayout Layout,
             MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode>
    CopyBufferToArrayImage::CopyBufferToArrayImage(Image<ImgType, ImgMemProp, Layout>& image, Buffer<MemType, BufMemProp, ShareMode>& buffer, std::uint32_t target_idx)
        : CopyBufferToArrayImage(image, &buffer, Layout, target_idx) {}
} // namespace VSL_NAMESPACE::command

#endif //AHO_ALL_COPY_BUFFER_TO_IMAGE_H
