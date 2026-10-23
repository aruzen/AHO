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
        std::size_t row_stride = 0;
        VSL_NAMESPACE::data_format::Format format = VSL_NAMESPACE::data_format::UNDEFINED;
        // 画素は隙間なく格納する。エンコードや色変換は呼び出し側で行う。
        std::vector<std::byte> bytes;
    };
    struct ImageReadbackInfo {
        // requestIdは共有する読み戻しオブジェクト内で一意。frameIdは呼び出し側で指定する。
        std::uint64_t request_id = 0, frame_id = 0;
        VSL_NAMESPACE::ImageRegion region;
        VSL_NAMESPACE::data_format::Format format = VSL_NAMESPACE::data_format::UNDEFINED;
        std::size_t row_stride = 0, byte_size = 0;
    };
    class ImageReadbackTicket {
        struct Data;
        std::shared_ptr<Data> _data;
        explicit ImageReadbackTicket(std::shared_ptr<Data> data);
        friend class ImageReadback;

    public:
        ImageReadbackTicket() = default;
        explicit operator bool() const;
        const ImageReadbackInfo& info() const;
        VSL_NAMESPACE::Completion::Status status() const;
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        void read(ImagePixels& destination, std::uint64_t timeout_ns = UINT64_MAX) const;
    };

    // スロットを上限数まで再利用する。チケットの全コピーを解放するまで、そのスロットを保持する。
    class ImageReadback {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit ImageReadback(VSL_NAMESPACE::CommandManager manager, std::size_t initial_capacity = 0,
                               std::size_t max_requests = 1);
        ImageReadback(VSL_NAMESPACE::CommandManager manager, VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout current_layout,
                      VSL_NAMESPACE::ImageRegion region = {});

        // 従来の最新結果を読むAPI。要求を受理すると、完了済みの結果を置き換える。
        bool capture(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout current_layout, VSL_NAMESPACE::ImageRegion region = {});
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        void read(ImagePixels& destination, std::uint64_t timeout_ns = UINT64_MAX) const;

        // 従来APIの最新結果を手放す。進行中のGPU処理は完了まで継続する。
        void discard_latest();

        // 空きスロットがなければnulloptを返す。完了済みでも保持中のチケットはスロットを占有する。
        std::optional<ImageReadbackTicket> capture_ticket(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout current_layout,
                                                          VSL_NAMESPACE::ImageRegion region = {}, std::uint64_t frame_id = 0);
        // RenderPassEndの後に記録する。追加の送信やCPUでの完了待ちは行わない。
        std::optional<ImageReadbackTicket> record(VSL_NAMESPACE::ComputePhase& phase, VSL_NAMESPACE::ImageAccessor image,
                                                  VSL_NAMESPACE::ImageLayout current_layout, VSL_NAMESPACE::ImageRegion region = {},
                                                  std::uint64_t frame_id = 0);
        // 確保済みの全スロットのGPUバッファ容量を合計する。
        std::size_t capacity() const;
    };

    inline ImageReadbackTicket::ImageReadbackTicket(std::shared_ptr<Data> data) : _data(std::move(data)) {}

    inline ImageReadbackTicket::operator bool() const {
        return bool(_data);
    }
} // namespace AHO_NAMESPACE
