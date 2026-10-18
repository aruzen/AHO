#pragma once
#include "define.hpp"
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/image.hpp>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>
#include <utility>

namespace VSL_NAMESPACE {
    struct ComputePhase;
}
namespace AHO_NAMESPACE {
    struct ImagePixels {
        std::uint32_t width = 0, height = 0;
        std::size_t rowStride = 0;
        vsl::data_format::___Format format = vsl::data_format::Undefined;
        // 画素は隙間なく格納する。エンコードや色変換は呼び出し側で行う。
        std::vector<std::byte> bytes;
    };
    struct ImageReadbackInfo {
        // requestIdは共有する読み戻しオブジェクト内で一意。frameIdは呼び出し側で指定する。
        std::uint64_t requestId = 0, frameId = 0;
        vsl::ImageRegion region;
        vsl::data_format::___Format format = vsl::data_format::Undefined;
        std::size_t rowStride = 0, byteSize = 0;
    };
    class ImageReadbackTicket {
        struct Data;
        std::shared_ptr<Data> _data;
        explicit ImageReadbackTicket(std::shared_ptr<Data> data) : _data(std::move(data)) {}
        friend class ImageReadback;

    public:
        ImageReadbackTicket() = default;
        explicit operator bool() const { return bool(_data); }
        const ImageReadbackInfo &info() const;
        vsl::Completion::Status status() const;
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        void read(ImagePixels &destination, std::uint64_t timeout_ns = UINT64_MAX) const;
    };

    // スロットを上限数まで再利用する。チケットの全コピーを解放するまで、そのスロットを保持する。
    class ImageReadback {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit ImageReadback(vsl::CommandManager manager, std::size_t initialCapacity = 0,
                               std::size_t maxRequests = 1);
        ImageReadback(vsl::CommandManager manager, vsl::ImageAccessor image, vsl::ImageLayout currentLayout,
                      vsl::ImageRegion region = {});

        // 従来の最新結果を読むAPI。要求を受理すると、完了済みの結果を置き換える。
        bool capture(vsl::ImageAccessor image, vsl::ImageLayout currentLayout, vsl::ImageRegion region = {});
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        void read(ImagePixels &destination, std::uint64_t timeout_ns = UINT64_MAX) const;

        // 従来APIの最新結果を手放す。進行中のGPU処理は完了まで継続する。
        void discardLatest();

        // 空きスロットがなければnulloptを返す。完了済みでも保持中のチケットはスロットを占有する。
        std::optional<ImageReadbackTicket> captureTicket(vsl::ImageAccessor image, vsl::ImageLayout currentLayout,
                                                         vsl::ImageRegion region = {}, std::uint64_t frameId = 0);
        // RenderPassEndの後に記録する。追加の送信やCPUでの完了待ちは行わない。
        std::optional<ImageReadbackTicket> record(vsl::ComputePhase &phase, vsl::ImageAccessor image,
                                                  vsl::ImageLayout currentLayout, vsl::ImageRegion region = {},
                                                  std::uint64_t frameId = 0);
        // 確保済みの全スロットのGPUバッファ容量を合計する。
        std::size_t capacity() const;
    };
}
