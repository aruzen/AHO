#pragma once
#include "define.hpp"
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/image.hpp>
#include <cstddef>
#include <memory>
#include <vector>

namespace AHO_NAMESPACE {
    struct ImagePixels {
        std::uint32_t width = 0, height = 0;
        std::size_t rowStride = 0;
        VSL_NAMESPACE::data_format::___Format format = VSL_NAMESPACE::data_format::Undefined;
        // 画素は隙間なく格納する。エンコードや色変換は呼び出し側で行う。
        std::vector<std::byte> bytes;
    };

    // 同時に転送できるのは1件。コピーしたオブジェクトは転送用リソースを共有する。
    class ImageReadback {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit ImageReadback(VSL_NAMESPACE::CommandManager manager, std::size_t initialCapacity = 0);
        // 初回の読み戻しも開始する。以降のフレームでは同じオブジェクトのcapture()を使う。
        ImageReadback(VSL_NAMESPACE::CommandManager manager, VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout currentLayout,
                      VSL_NAMESPACE::ImageRegion region = {});

        // 同じグラフィックスキューへの描画送信後に呼ぶ。前の転送が進行中ならfalseを返す。
        // 要求を受理すると前の結果を置き換える。バッファは容量が不足したときだけ拡張する。
        bool capture(VSL_NAMESPACE::ImageAccessor image, VSL_NAMESPACE::ImageLayout currentLayout, VSL_NAMESPACE::ImageRegion region = {});
        bool ready() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        // 出力先の画素配列の容量も再利用する。
        void read(ImagePixels& destination, std::uint64_t timeout_ns = UINT64_MAX) const;
        std::size_t capacity() const;
    };
} // namespace AHO_NAMESPACE
