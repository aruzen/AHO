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
#include "../../lifetime/buffer_view.hpp"
#include "copy_image_to_buffer.hpp"

namespace VSL_NAMESPACE::command {
    struct CopyBufferToImage : public Command {
        CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer, VSL_NAMESPACE::ImageLayout layout);
        CopyBufferToImage(ImageAccessor image, BufferAccessor* buffer, ImageLayout layout, ImageRegion region, std::size_t offset = 0);
        // offsetはview内の相対値。retentionは転送元だけの保持契約。
        CopyBufferToImage(ImageAccessor image, BufferView source, ImageLayout layout, ImageRegion region = {}, std::size_t offset = 0,
                          ResourceUse constraint = {}, ResourceRetention retention = ResourceRetention::COMPLETION);

        template<VSL_NAMESPACE::ImageType img_type, VSL_NAMESPACE::MemoryProperty img_mem_prop, VSL_NAMESPACE::ImageLayout layout_,
                 MemoryType mem_type, MemoryProperty buf_mem_prop, SharingMode share_mode>
        CopyBufferToImage(Image<img_type, img_mem_prop, layout_>& image, Buffer<mem_type, buf_mem_prop, share_mode>& buffer);

        VSL_NAMESPACE::ImageLayout layout;
        ImageAccessor image;
        BufferAccessor* buffer;
        ImageRegion region;
        std::size_t offset = 0;

        ImageCopyInfo description() const;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        friend struct ImageUpload;
        impl::BufferImpl* _source;
        ResourceUse _use;
        std::size_t _source_offset = 0, _source_bytes = 0;
        ResourceRetention _retention = ResourceRetention::SUBMISSION;
        ImageCopyInfo prepare(const CommandBuffer& buffer, const CommandManager& manager) const;
        void track(const NativeRecordingContext& recording) const;
        void record(const ImageCopyInfo& info, const NativeRecordingContext& recording) const;
    };

    struct CopyBufferToArrayImage : public Command {
        CopyBufferToArrayImage(ImageAccessor image, BufferAccessor* buffer, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx);

        template<VSL_NAMESPACE::ImageType img_type, VSL_NAMESPACE::MemoryProperty img_mem_prop, VSL_NAMESPACE::ImageLayout layout_,
                 MemoryType mem_type, MemoryProperty buf_mem_prop, SharingMode share_mode>
        CopyBufferToArrayImage(Image<img_type, img_mem_prop, layout_>& image, Buffer<mem_type, buf_mem_prop, share_mode>& buffer, std::uint32_t target_idx);

        VSL_NAMESPACE::ImageLayout layout;
        ImageAccessor image;
        BufferAccessor* buffer{};
        std::uint32_t target_idx{};

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);

    private:
        lifetime::OwnedRef<impl::BufferImpl> _source;
    };

    template<VSL_NAMESPACE::ImageType img_type, VSL_NAMESPACE::MemoryProperty img_mem_prop, VSL_NAMESPACE::ImageLayout layout_,
             MemoryType mem_type, MemoryProperty buf_mem_prop, SharingMode share_mode>
    CopyBufferToImage::CopyBufferToImage(Image<img_type, img_mem_prop, layout_>& image, Buffer<mem_type, buf_mem_prop, share_mode>& buffer)
        : CopyBufferToImage(image, &buffer, layout_) {}

    template<VSL_NAMESPACE::ImageType img_type, VSL_NAMESPACE::MemoryProperty img_mem_prop, VSL_NAMESPACE::ImageLayout layout_,
             MemoryType mem_type, MemoryProperty buf_mem_prop, SharingMode share_mode>
    CopyBufferToArrayImage::CopyBufferToArrayImage(Image<img_type, img_mem_prop, layout_>& image, Buffer<mem_type, buf_mem_prop, share_mode>& buffer, std::uint32_t target_idx)
        : CopyBufferToArrayImage(image, &buffer, layout_, target_idx) {}
} // namespace VSL_NAMESPACE::command

#endif //AHO_ALL_COPY_BUFFER_TO_IMAGE_H
