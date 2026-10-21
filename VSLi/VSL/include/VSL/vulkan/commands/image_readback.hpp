#pragma once
#include "copy_image_to_buffer.hpp"

namespace VSL_NAMESPACE::command {
    // レイアウト遷移、バッファへのコピー、CPUへの可視性確保、レイアウト復元を記録する。
    // レンダーパスの外で使う。送信と完了待ちは呼び出し側で行う。
    // Phaseに記録すると画像と転送先を送信側が保持する。直接記録する場合は呼び出し側で保持する。
    struct ImageReadback : public Command {
        ImageReadback(BufferAccessor* destination, ImageAccessor image, ImageLayout current_layout,
                      ImageRegion region = {});
        ImageReadback(std::shared_ptr<impl::BufferImpl> destination, ImageAccessor image,
                      ImageLayout current_layout, ImageRegion region = {});

        CopyImageToBuffer copy;
        ImageLayout current_layout;

        static ImageCopyInfo Describe(ImageAccessor image, ImageLayout current_layout, ImageRegion region = {});
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
} // namespace VSL_NAMESPACE::command
