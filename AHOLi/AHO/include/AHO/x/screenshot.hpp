#pragma once
#include "../image_readback.hpp"
#include <filesystem>
#include <memory>

namespace AHO_NAMESPACE {
    struct DrawPhase;
    namespace window {
        struct Window;
    }
} // namespace AHO_NAMESPACE

namespace AHO_NAMESPACE::x {
    // RGBA8またはBGRA8の画素をPNGで保存する。行の順序とアルファ値を維持する。
    void saveScreenshot(const ImagePixels& pixels, const std::filesystem::path& path);

    // 描画ループの外で作成し、転送用バッファを再利用する。コピーすると撮影結果を共有する。
    class Screenshot {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit Screenshot(window::Window window);

        // 終了コールバックの後に同じPhaseへ読み戻しを記録する。前の撮影が進行中ならfalse。
        // 呼び出し後はphase.finishを置き換えない。受理すると前の結果を手放す。
        bool capture(DrawPhase& phase);
        bool ready() const;

        // Phaseの送信後に呼ぶ。GPUの完了を待ち、最新の受理済み結果を読む。
        ImagePixels read(std::uint64_t timeout_ns = UINT64_MAX) const;
        void save(const std::filesystem::path& path, std::uint64_t timeout_ns = UINT64_MAX) const;
    };
} // namespace AHO_NAMESPACE::x
