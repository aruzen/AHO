#pragma once
#include "copy_buffer_to_image.hpp"

namespace VSL_NAMESPACE::command {
    // レンダーパス外で記録する。Undefinedからの初回更新では画像全体をゼロで埋める。
    // コピー後はShaderReadOnlyOptimalへ遷移する。送信と完了待ちは呼び出し側で行う。
    struct ImageUpload : __Command {
        ImageUpload(BufferAccessor* source, ImageAccessor image, ImageLayout currentLayout,
                    ImageRegion region = {}, std::size_t offset = 0);
        CopyBufferToImage copy;
        ImageLayout currentLayout;
        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager) override;
    };
} // namespace VSL_NAMESPACE::command
