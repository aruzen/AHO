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
    struct ImageCopyInfo {
        ImageRegion region;
        data_format::___Format format = data_format::Undefined;
        std::size_t rowStride = 0, byteSize = 0;
    };

    struct CopyImageToBuffer : public __Command {
        CopyImageToBuffer(BufferAccessor *buffer, ImageAccessor image);
        // ミップレベル0の画素をバッファの先頭へ隙間なくコピーするコマンドを記録する。送信やCPU待機は行わない。
        // hostReadはCPUへの可視性を確保する。読み取り前には別途GPUの完了待ちが必要。
        CopyImageToBuffer(BufferAccessor *buffer, ImageAccessor image, ImageLayout layout,
                          ImageRegion region = {}, bool hostRead = false);
        CopyImageToBuffer(std::shared_ptr<_impl::Buffer_impl> buffer, ImageAccessor image, ImageLayout layout,
                          ImageRegion region = {}, bool hostRead = false);

        template<MemoryType MemType, MemoryProperty BufMemProp, SharingMode ShareMode,
                 vsl::ImageType ImgType, vsl::MemoryProperty ImgMemProp, vsl::ImageLayout Layout>
        CopyImageToBuffer(Buffer<MemType, BufMemProp, ShareMode> &buffer, Image<ImgType, ImgMemProp, Layout> &image,
                          ImageLayout layout = ImageLayout::General, ImageRegion region = {}, bool hostRead = false)
            : CopyImageToBuffer(&buffer, image, layout, region, hostRead){};

        ImageAccessor image;
        BufferAccessor *buffer;
        ImageLayout layout = ImageLayout::General;
        ImageRegion region;
        bool hostRead = false;

        static ImageCopyInfo describe(ImageAccessor image, ImageRegion region = {});
        ImageCopyInfo description() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        std::shared_ptr<_impl::Buffer_impl> destination_;
    };
}

#endif
