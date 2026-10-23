# RenderPass構成と画像読み戻し

更新日: 2026-10-18。

## 責務

用途別の型を増やさず、IDPicking・オフスクリーン描画・スクショの基盤を構成する。

| 型 | 責務 |
| --- | --- |
| RenderPassDescription | Attachmentのformat/sample/load/store/layout、subpass、dependencyを所有する構成データ。 |
| RenderPass | 構成を検証し、変更不可の構成とネイティブハンドルを保持する。Swapchain不要。 |
| FrameBuffer | Attachmentに実画像と幅・高さ・レイヤー数を結び付け、画像とRenderPassを共有保持する。 |
| RenderPassBegin | 今回の対象index、描画領域、Attachmentごとの型別clearを指定する。 |
| GraphicsPipeline | subpass番号、カラー出力ごとのblend/write mask、深度・stencil、sample数を指定する。 |
| vsl::command::ImageReadback | 指定領域・レイヤーの転送、host可視性、layout復帰を記録する。送信・CPU待機・リソース確保は行わない。 |
| aho::ImageReadback | VSLコマンドを送信し、転送リソースを再利用する便利クラス。完了待ち・CPUデータ取得も担当。 |

IDの割当、画像の確保・リサイズ、PNG等への保存、UI操作は利用側の責任とする。

## カラーと整数IDを出力する例

```cpp
#include <VSL/vsl.hpp>
#include <cstring>
using namespace vsl;

Image<ImageType::COLOR_ATTACHMENT | ImageType::TRANSFER_SRC> color(
    device, width, height, data_format::UNSIGNED_NORMALIZED8_RGBA);
Image<ImageType::COLOR_ATTACHMENT | ImageType::TRANSFER_SRC> ids(
    device, width, height, data_format::UNSIGNED_INT);

RenderPassDescription desc;
const auto color_id = desc.add_color({
    .format = color.get_format(), .final_layout = ImageLayout::GENERAL,
});
const auto object_id = desc.add_color({
    .format = ids.get_format(), .final_layout = ImageLayout::GENERAL,
});
RenderPass pass(device, desc);
FrameBuffer target(pass, width, height, {color, ids});
```

AttachmentIdは `desc.attachments` の位置。FrameBufferの画像とclear値はこの順序に合わせる。subpassの `colors` の位置はfragment shaderの出力locationとなる。この例はlocation 0がカラー、1がuintのID。

```cpp
namespace pl = vsl::pipeline_layout;
pl::ColorBlend blends(std::vector<pl::ColorBlendAttachment>{
    {},
    {.enabled = false, .write_mask = pl::ColorWriteMask::R},
});
// PipelineLayoutへblendsを注入する。整数出力のblendは無効にする。
{
    auto phase = manager.start_phase<ComputePhase>();
    phase << command::RenderPassBegin(target, {
        ClearColor{0.f, 0.f, 0.f, 1.f}, ClearUint{0, 0, 0, 0},
    });
    // pipeline、viewport、scissorを設定し、描画する。
    phase << command::RenderPassEnd{};
} // 記録した処理を送信する。
```

clear値はClearColor/ClearInt/ClearUint/ClearDepthStencilのvariant。省略時はformatに応じた0、深度は1を生成する。明示指定時はAttachment数と同じ個数を渡し、CLEARを使うAttachmentでは型も検証する。RenderAreaで部分領域を指定でき、幅・高さが両方0なら全体を使う。

## 深度、MSAA、複数subpass

- 深度画像は `Image<ImageType::DEPTH_STENCIL_ATTACHMENT>` と `data_format::DEPTH32_FLOAT` 等で作る。ImageViewのaspectはformatから選ぶ。`set_depth_stencil` で接続し、`pl::DepthStencil` で深度テスト・書込み・比較、前面/背面stencilを設定する。
- Image、Attachment、`pl::Multisample` のSampleCountを一致させる。MSAAのresolve先は単一sampleのAttachmentとして追加し、`subpasses[i].resolves` にカラー出力と同じ順序で指定する。読み戻しはresolve先に対して行う。
- `add_attachment` で役割を固定せず登録できる。subpassesのcolors/inputs/resolves/depth_stencil/preserveに参照を設定する。未使用スロットはUNUSED_ATTACHMENT。
- `GraphicsPipeline(layout, pass, subpass_index)` で対象を選び、描画中に `command::NextSubpass{}` で次へ進む。
- dependenciesでstage/accessとsubpass番号を明示できる。既定はexternal→0と0→externalの保守的な依存関係。複数subpassでは間の依存関係と、最後のsubpass→externalを追加する。
- 構造上の整合は検証するが、Vulkanのすべてのvalid usage、formatやfeatureのGPU対応確認を代替しない。

## AHOの再利用可能な読み戻し

`aho::ImageReadback` を描画ループの外で1つ作り、producerを同じmanagerのgraphics queueへ送信してからcaptureする。captureには実際の現在layoutを渡す。

```cpp
#include <AHO/image_readback.hpp>

aho::ImageReadback reader(manager); // CommandPool/CommandBuffer/Fenceはここで一度作る。
aho::ImagePixels pixels;
// 描画ループ内。以前の結果を先に回収してから、今回のコピーを要求する。
if (reader.ready()) {
    reader.read(pixels); // CPU側vectorの容量も再利用する。
}
if (!reader.capture(color, ImageLayout::GENERAL)) {
    // 前の転送が進行中。このフレームの要求は受理されていない。
}
// 必要なタイミングで、最新の受理済み転送の完了を待って読むこともできる。
pixels = reader.read();

// 同じ読み戻しオブジェクトを部分領域や整数ID画像にも使える。
if (reader.capture(ids, ImageLayout::GENERAL, {x, y, 1, 1})) {
    reader.read(pixels);
    std::uint32_t id;
    std::memcpy(&id, pixels.bytes.data(), sizeof(id));
}
```

- 元画像にはTRANSFER_SRC usageが必要。非圧縮カラーformatのmip 0、指定した1レイヤー、sample数1が対象。深度、サイズを解決できないformat、MSAA画像そのものは拒否する。
- `ImageRegion{x,y,width,height,layer}` を指定する。領域省略は全画面、layer省略は0。範囲外や片方だけ0の寸法は拒否する。
- 同時に進行できる転送は1件。captureは進行中ならfalseを返し、待機や結果の上書きを行わない。完了済みの結果は次のcapture受理まで繰り返しreadできる。コピーしたworkerハンドルも同じ状態を共有する。
- CommandPool/CommandBuffer/Fenceを再利用する。host-visible bufferは必要容量が増えたときだけ再確保する。constructorのinitial_capacityで先に容量を用意でき、capacity()で現在の容量を取得できる。
- mapはbuffer確保時に行い、再確保か破棄まで維持する。non-coherentメモリーは完了後に必要範囲をatom境界へ切り上げてinvalidateする。read(destination)はCPU側vectorの容量も再利用する。
- source画像は転送完了まで保持する。描画記録中のmanagerのCommandBufferをresetしない。最後のworkerハンドルを破棄するときは、進行中の自分の転送完了を待って解放する。
- captureはCPUのGPU完了待ちを行わない。wait(timeout_ns)はタイムアウトでfalse、read(timeout_ns)は例外。未captureのreadyはfalse、wait/readは例外。
- bytesはnative formatの順序・色空間のまま行間paddingなし。BGRA→RGBA、上下反転、色空間変換、画像エンコードは利用側で行う。
- queueへの並行CPUアクセス、別queueとのSemaphore/所有権移譲、同じworkerへの並行アクセスは利用側で同期する。未送信producerの結果は取得できない。

## VSLの読み戻しコマンド

描画と同じPhaseへ記録する場合は `vsl::command::ImageReadback` を使う。既存の `ChangeImageBarrier` と `CopyImageToBuffer` をまとめ、layout遷移・領域転送・host可視性・元layoutへの復帰だけを記録する。リソース確保・queueへの送信・CPU待機は行わない。必要なら既存2コマンドを個別に使える。

```cpp
#include <VSL/vulkan/commands/image_readback.hpp>

// completion_fenceはmanagerの各bufferに対応し、初回はsignal済みで用意する。
StagingBuffer<MemoryType::TRANSFER_DESTINATION> destination(
    device, std::size_t(width) * height * color.get_format().size());
{
    auto phase = manager.start_phase<ComputePhase>(
        std::nullopt, std::nullopt, completion_fence);
    // colorへの描画とRenderPassEndまでを、このPhase内で記録する。
    phase << command::ImageReadback(&destination, color, ImageLayout::GENERAL);
}
if (!completion_fence.wait(manager.get_current_buffer_idx(), UINT64_MAX))
    throw std::runtime_error("Image transfer did not complete");
auto mapped = destination.data();
// mappedの寿命中はmapを維持する。画像・bufferもGPU完了まで保持する。
```

`ImageReadback::Describe(image, layout, region)` は正規化済み領域・format・row_stride・byte_sizeを返す。CopyImageToBufferにもDescribeがあり、転送先を確保する前に必要容量を取得できる。AHOもこの情報とVSLコマンドを使用する。VSLはAHOへ依存しない。

## 既存commandによる読み戻し

下位の転送・バリア制御が必要な場合は、既存の `ChangeImageBarrier` と `CopyImageToBuffer` を個別に組み合わせられる。

```cpp
#include <VSL/vulkan/commands/change_image_barrier.hpp>
#include <VSL/vulkan/commands/copy_image_to_buffer.hpp>

// completion_fenceはmanagerの各bufferに対応し、初回はsignal済みで用意する。
StagingBuffer<MemoryType::TRANSFER_DESTINATION> destination(
    device, std::size_t(width) * height * color.get_format().size());
{
    auto phase = manager.start_phase<ComputePhase>(
        std::nullopt, std::nullopt, completion_fence);
    // このPhase内でcolorへ描画し、RenderPassEndまで記録する。
    phase << command::ChangeImageBarrier(
                 color, ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC_OPTIMAL)
          << command::CopyImageToBuffer(
                 &destination, color, ImageLayout::TRANSFER_SRC_OPTIMAL, {}, true)
          << command::ChangeImageBarrier(
                 color, ImageLayout::TRANSFER_SRC_OPTIMAL, ImageLayout::GENERAL);
}
if (!completion_fence.wait(manager.get_current_buffer_idx(), UINT64_MAX))
    throw std::runtime_error("Image transfer did not complete");
auto mapped = destination.data();
// mapped.dataから画素を読む。mappedの寿命中はmapを維持する。
```

- `ChangeImageBarrier(image, old_layout, new_layout, base_layer, layer_count)` で対象レイヤーを限定できる。従来の3引数版は全レイヤー。layer_countが0ならbase_layer以降の残り全部。mipは0を対象とする。
- GENERAL/SHADER_READ_ONLY_OPTIMAL/TRANSFER_DST_OPTIMAL/PRESENT/COLOR_ATTACHMENT_OPTIMALとTRANSFER_SRC_OPTIMAL間の遷移に対応する。queue familyの所有権移譲は行わない。
- `CopyImageToBuffer(buffer, image, layout, region, host_read)` は、GENERALまたはTRANSFER_SRC_OPTIMALから指定領域・1レイヤーをbuffer offset 0へ密にコピーする。`ImageRegion` は `image.hpp` に配置する。
- host_read=trueは転送書込み→CPU読取りのメモリー依存を記録する。送信やCPU待機は行わない。Fence等で転送完了を待ってからmapし、non-coherentメモリーではinvalidateも行う。例のStagingBufferはHOST_COHERENTを要求する。
- 画像・転送先buffer・記録したCommandBufferはGPU完了まで利用側が保持する。記録中のRenderPassの外で呼び出す。領域・レイヤー・転送先容量・deviceの不一致は記録前に拒否する。
- 既存の2引数版と型付きの呼出しは、GENERALからmip 0・layer 0の全画面をコピーする従来の動作を維持する。

## Swapchain、互換性、寿命

Swapchain末尾のusage引数に `ImageType::COLOR_ATTACHMENT | ImageType::TRANSFER_SRC` を渡すと、Surfaceの対応usageを確認して作成する。非対応なら例外にし、暗黙に用途を落とさない。その環境では転送可能なオフスクリーン画像へ描画する。

直接Swapchain画像を読む場合は、取得済みの画像を選び、描画送信とpresentの間へ転送を配置する。今回の変更は既存DefaultPhaseのsubmitとpresentの分離を行わない。その経路へのスクショ機能の接続には送信・表示順序の連携が別途必要。

- RenderPass(swapchain)、FrameBuffer(swapchain, pass)、旧RenderPassBegin(pass, framebuffer, color)を維持する。
- IDPickingRenderPass/IDPickingRenderPassBegin/pl::IDPickingは共通実装へ委譲する互換ラッパー。新しい用途では追加不要。
- IDPickingRenderPass::readは描画時に取得したswapchain画像番号からIDを読む。command bufferのslot番号と画像番号は独立している。取得番号がない従来の呼び出しは現在のslot番号へフォールバックする。読み取りは描画の完了後、同じmanagerで次のPhaseを開始する前に行う。
- 新FrameBufferは1対象でBeginのindexは既定0。旧一括FrameBufferでも新Beginのtarget_indexで取得画像を明示できる。旧Beginの自動選択は取得済み画像番号を優先し、ない場合は従来のslot番号を使う。
- FrameBufferは画像とRenderPassを保持する。借用Swapchain画像はSwapchainとSurfaceを保持する。描画送信後のFrameBuffer/Pipeline等は利用側がGPU完了まで保持する。
- aho::ImageReadbackが保持するのは転送に必要なリソースであり、描画リソース全体のGPU寿命管理を代替しない。
- リサイズは利用側で新画像とFrameBufferを作る。同じformat/sample構成ならRenderPassを再利用できる。旧リソースはGPU利用完了後に解放する。

## 検証対象

公開APIのみで、カラー＋整数ID同時出力、signed/unsigned clear、深度によるfragment棄却、複数subpass、MSAA resolve、配列レイヤー、LOADによる保持、全画面/部分領域読み戻し、layout復帰、元画像の寿命保持を検証する。これらはvalidation Layerのerrorを捕捉し、リソース破棄時も含めテスト失敗へ反映する。

既存Render fixtureで行っていた内部RenderPass差し替えを撤去した。修正前に失敗したIDPickingの基底Accessor、深度state、配列画像サイズ、3定数のformatサイズの回帰テストを通常実行へ戻した。

## 2026-10-17 検証結果

Apple M1 / MoltenVK、Apple Clang 17、Vulkan SDK 1.4.309.0、Debugで実行。

- VSL通常実行: 184件成功、失敗0、環境skip 0。既存の未修正不具合26件はDISABLEDのまま。
- うち共通描画・読み戻し9件と実Swapchain読み戻し1件はvalidation Layerを有効化し、errorをテスト失敗へ反映。10件とも成功。
- 既存の4件の回帰テストは変更前に失敗を確認し、変更後に通常テストとして成功。
- AHO側の変更を取り込んだbase `0cfeb8a` 上で `build-tests` が成功。
- 全体CTestは645登録中76件失敗で、失敗はすべてAHO側（CPU、compile/link契約、描画フェーズ3件のtimeout）。VSL側の失敗は0。未修正のAHO契約や描画フェーズの問題まで解消したという結果ではない。
- 描画フェーズの代表1件は元の `cmake-build-tests` でも45秒timeoutと `VUID-VkPresentInfoKHR-pImageIndices-01430` を再現。取得画像とpresent画像番号の不一致は今回の抽象化とは別の残課題として扱う。
- 画像エンコード、UI操作、Windows/Linux、深度画像自体の読み戻し、別queue間転送、自動layout追跡は今回の対象外。

```sh
cmake --build build-vsl-abstraction --target vsl-tests --parallel 6
build-vsl-abstraction/VSLi/VSL/test/vsl-test
# CTestでも同じケースを実行できる。
ctest --test-dir build-vsl-abstraction --output-on-failure -R '^VSL\.'
```

## 2026-10-18 既存commandへの委譲時の検証（AHO移行前）

- `vsl-tests` の再ビルドが成功。VSL全体189件成功、失敗0、skip 0。既知不具合26件はDISABLEDを維持。
- `ChangeImageBarrier` / `CopyImageToBuffer` / `ImageReadback` / 実Swapchain読み戻しの関連18件で、validation Layerのerrorを捕捉して成功。
- 描画と同じPhase内での領域読み戻し、異なる内容を持つ配列レイヤーの選択、他レイヤーのlayout維持、転送先容量と範囲外の拒否を追加検証。
- 関連4ヘッダーの単独includeコンパイルが成功。読み戻しの専用CommandBufferもGPU完了後に解放してから専用poolを破棄する。

## 2026-10-18 AHOへの移動とリソース再利用の検証

- `build-tests` が成功。VSL 182件とAHO読み戻し13件、新AHOヘッダーの単独include 1件の計196件が成功。失敗0、環境skip 0。既存のVSL不具合26件はDISABLEDを維持する。
- 読み戻しの10件をVSLからAHOへ移し、再利用・容量拡張・busy拒否の3件と、VSLコマンド専用の3件を追加した。AHOの13件とVSLコマンド3件はvalidation Layerのerrorを捕捉する。
- 24フレームと部分領域1回の取得を通して、Vulkan呼出しを一時的に計数。転送先buffer作成1回、Fence作成1回、map 1回。CommandPool作成・CommandBuffer割当は各2回で、描画managerの初期化1回とreadbackの初期化1回に対応する。各captureでの作り直しはない。
- busy拒否はTimeline Semaphoreで前の転送を確実に進行中へ保ち、capture=false、wait(0)=false、元の結果が保持されることを実GPUで確認した。
- AHOとVSLでVulkan SDK 1.4.309.0のヘッダーを統一した。AHOがHomebrew側の別バージョンを先に参照する設定では、Vulkan-Hppのheader version assertionが発生していた。
- 新AHO/VSLヘッダー4件の単独includeと、CMake/Visual Studioプロジェクトの参照先を確認した。AHO全体の既知の描画フェーズ問題の解消は、この結果に含めない。

### 毎回作成と再利用の時間比較

Apple M1 / MoltenVK、ライブラリーはDebug、計測コードは-O2。30回warmup後、1pxは各2000回、1080p RGBA8は各200回を計測。前後の順を交互に変えた5組の中央値。両方でCPU側ImagePixelsのvector容量を再利用し、GPU側リソースの使い方を比較した。

| 対象 | 毎回作成: 準備〜submit | 再利用: 準備〜submit | 毎回作成: 読取り完了まで | 再利用: 読取り完了まで |
| --- | ---: | ---: | ---: | ---: |
| 1px | 23.0 µs | 14.4 µs | 192 µs | 165 µs |
| 1920×1080 | 34.6 µs | 17.3 µs | 1.58 ms | 0.76 ms |

GPU timestampによる純粋な転送時間ではなく、待機・CPUコピー・毎回作成側の後始末も含む。Releaseや他GPUで同じ比率になるという保証ではない。AHOのcaptureは追加submitを1回行う。既存Phaseへ送信を統合する用途ではVSLコマンドを直接使う。
