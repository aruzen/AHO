#pragma once
#include "copy_image_to_buffer.hpp"

namespace VSL_NAMESPACE::command {
    // レイアウト遷移、バッファへのコピー、CPUへの可視性確保、レイアウト復元を記録する。
    // レンダーパスの外で使う。送信、完了待ち、リソースの寿命管理は呼び出し側で行う。
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
