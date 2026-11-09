#ifndef AHO_ALL_COPY_IMAGE_TO_BUFFER_H
#define AHO_ALL_COPY_IMAGE_TO_BUFFER_H

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../descriptor.hpp"
#include "../pipeline.hpp"

#include "../image.hpp"
#include "../buffer.hpp"
#include "../../lifetime/buffer_view.hpp"

namespace VSL_NAMESPACE {
    class NativeRecordingContext;
}
namespace VSL_NAMESPACE::command {
    struct ImageCopyInfo {
        ImageRegion region;
        data_format::Format format = data_format::UNDEFINED;
        std::size_t row_stride = 0, byte_size = 0;
    };

    struct CopyImageToBuffer : public Command {
        CopyImageToBuffer(BufferAccessor* buffer, ImageAccessor image);
        // ミップレベル0の画素をバッファの先頭へ隙間なくコピーするコマンドを記録する。送信やCPU待機は行わない。
        // hostReadはCPUへの可視性を確保する。読み取り前には別途GPUの完了待ちが必要。
        CopyImageToBuffer(BufferAccessor* buffer, ImageAccessor image, ImageLayout layout,
                          ImageRegion region = {}, bool host_read = false);
        // コピー先はviewの範囲に限定し、constraintは追加の論理的な更新制約とする。
        CopyImageToBuffer(BufferView destination, ImageAccessor image, ImageLayout layout,
                          ImageRegion region = {}, bool host_read = false, ResourceUse constraint = {},
                          ResourceRetention retention = ResourceRetention::COMPLETION);

        template<MemoryType mem_type, MemoryProperty buf_mem_prop, SharingMode share_mode,
                 VSL_NAMESPACE::ImageType img_type, VSL_NAMESPACE::MemoryProperty img_mem_prop, VSL_NAMESPACE::ImageLayout layout_>
        CopyImageToBuffer(Buffer<mem_type, buf_mem_prop, share_mode>& buffer, Image<img_type, img_mem_prop, layout_>& image,
                          ImageLayout layout = ImageLayout::GENERAL, ImageRegion region = {}, bool host_read = false);

        ImageAccessor image;
        BufferAccessor* buffer;
        ImageLayout layout = ImageLayout::GENERAL;
        ImageRegion region;
        bool host_read = false;

        static ImageCopyInfo Describe(ImageAccessor image, ImageRegion region = {});
        ImageCopyInfo description() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        friend struct ImageReadback;
        impl::BufferImpl* _destination;
        ResourceUse _use;
        std::size_t _destination_offset = 0, _destination_size = 0;
        ResourceRetention _retention = ResourceRetention::SUBMISSION;
        ImageCopyInfo prepare(const CommandBuffer& buffer, const CommandManager& manager) const;
        void track(const NativeRecordingContext& recording) const;
        void record(const ImageCopyInfo& info, const NativeRecordingContext& recording) const;
    };

    template<MemoryType mem_type, MemoryProperty buf_mem_prop, SharingMode share_mode,
             VSL_NAMESPACE::ImageType img_type, VSL_NAMESPACE::MemoryProperty img_mem_prop, VSL_NAMESPACE::ImageLayout layout_>
    CopyImageToBuffer::CopyImageToBuffer(Buffer<mem_type, buf_mem_prop, share_mode>& buffer, Image<img_type, img_mem_prop, layout_>& image,
                                         ImageLayout layout, ImageRegion region, bool host_read)
        : CopyImageToBuffer(&buffer, image, layout, region, host_read) {}
} // namespace VSL_NAMESPACE::command

#endif
