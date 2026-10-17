# RenderPass構成と画像読み戻し

更新日: 2026-10-17。

## 責務

用途別の型を増やさず、IDPicking・オフスクリーン描画・スクショの基盤を構成する。

| 型 | 責務 |
| --- | --- |
| RenderPassDescription | Attachmentのformat/sample/load/store/layout、subpass、dependencyを所有する構成データ。 |
| RenderPass | 構成を検証し、変更不可の構成とネイティブハンドルを保持する。Swapchain不要。 |
| FrameBuffer | Attachmentに実画像と幅・高さ・レイヤー数を結び付け、画像とRenderPassを共有保持する。 |
| RenderPassBegin | 今回の対象index、描画領域、Attachmentごとの型別clearを指定する。 |
| GraphicsPipeline | subpass番号、カラー出力ごとのblend/write mask、深度・stencil、sample数を指定する。 |
| ImageReadback | 指定領域・レイヤーを非同期転送し、完了待ちとCPUデータ取得を提供する。 |

IDの割当、画像の確保・リサイズ、PNG等への保存、UI操作は利用側の責任とする。

## カラーと整数IDを出力する例

```cpp
#include <VSL/vsl.hpp>
#include <cstring>
using namespace vsl;

Image<ImageType::ColorAttachment | ImageType::TransferSrc> color(
    device, width, height, data_format::UnsignedNormalized8RGBA);
Image<ImageType::ColorAttachment | ImageType::TransferSrc> ids(
    device, width, height, data_format::UnsignedInt);

RenderPassDescription desc;
const auto colorId = desc.addColor({
    .format = color.getFormat(), .finalLayout = ImageLayout::General,
});
const auto objectId = desc.addColor({
    .format = ids.getFormat(), .finalLayout = ImageLayout::General,
});
RenderPass pass(device, desc);
FrameBuffer target(pass, width, height, {color, ids});
```

AttachmentIdは `desc.attachments` の位置。FrameBufferの画像とclear値はこの順序に合わせる。subpassの `colors` の位置はfragment shaderの出力locationとなる。この例はlocation 0がカラー、1がuintのID。

```cpp
namespace pl = vsl::pipeline_layout;
pl::ColorBlend blends(std::vector<pl::ColorBlendAttachment>{
    {},
    {.enabled = false, .writeMask = pl::ColorWriteMask::R},
});
// PipelineLayoutへblendsを注入する。整数出力のblendは無効にする。
{
    auto phase = manager.startPhase<ComputePhase>();
    phase << command::RenderPassBegin(target, {
        ClearColor{0.f, 0.f, 0.f, 1.f}, ClearUint{0, 0, 0, 0},
    });
    // pipeline、viewport、scissorを設定し、描画する。
    phase << command::RenderPassEnd{};
} // 記録した処理を送信する。
```

clear値はClearColor/ClearInt/ClearUint/ClearDepthStencilのvariant。省略時はformatに応じた0、深度は1を生成する。明示指定時はAttachment数と同じ個数を渡し、Clearを使うAttachmentでは型も検証する。RenderAreaで部分領域を指定でき、幅・高さが両方0なら全体を使う。

## 深度、MSAA、複数subpass

- 深度画像は `Image<ImageType::DepthStencilAttachment>` と `data_format::Depth32Float` 等で作る。ImageViewのaspectはformatから選ぶ。`setDepthStencil` で接続し、`pl::DepthStencil` で深度テスト・書込み・比較、前面/背面stencilを設定する。
- Image、Attachment、`pl::Multisample` のSampleCountを一致させる。MSAAのresolve先は単一sampleのAttachmentとして追加し、`subpasses[i].resolves` にカラー出力と同じ順序で指定する。読み戻しはresolve先に対して行う。
- `addAttachment` で役割を固定せず登録できる。subpassesのcolors/inputs/resolves/depthStencil/preserveに参照を設定する。未使用スロットはUnusedAttachment。
- `GraphicsPipeline(layout, pass, subpassIndex)` で対象を選び、描画中に `command::NextSubpass{}` で次へ進む。
- dependenciesでstage/accessとsubpass番号を明示できる。既定はexternal→0と0→externalの保守的な依存関係。複数subpassでは間の依存関係と、最後のsubpass→externalを追加する。
- 構造上の整合は検証するが、Vulkanのすべてのvalid usage、formatやfeatureのGPU対応確認を代替しない。

## スクショとIDの共通読み戻し

producerを同じmanagerのgraphics queueへ送信してから、実際の現在layoutを渡す。

```cpp
ImageReadback screenshot(manager, color, ImageLayout::General);
// コンストラクターは専用の転送処理を送信し、CPUの完了待ちはしない。
if (screenshot.ready()) {
    ImagePixels pixels = screenshot.read();
}
// 必要な時点で完了を待ち、CPU所有のbyte列を取得する。
ImagePixels pixels = screenshot.read();
// pixels.width / height / format / rowStride / bytes

ImageReadback picking(manager, ids, ImageLayout::General, {x, y, 1, 1});
auto pixel = picking.read();
std::uint32_t id;
std::memcpy(&id, pixel.bytes.data(), sizeof(id));
```

- 元画像にはTransferSrc usageが必要。非圧縮カラーformatのmip 0、指定した1レイヤー、sample数1が対象。深度、サイズを解決できないformat、MSAA画像そのものは拒否する。
- `ImageRegion{x,y,width,height,layer}` を指定する。領域省略は全画面、layer省略は0。範囲外や片方だけ0の寸法は拒否する。
- 要求は専用CommandPool/CommandBuffer/Fence/host-visible bufferと元画像を保持する。描画記録中のCommandBufferをresetしない。ハンドルのコピーは同じ要求を共有し、完了後は繰り返しreadできる。
- 元layout→TransferSrc→元layoutの遷移と転送→hostの可視性を設定する。non-coherentメモリーでは割当全体をmapしてinvalidateする。
- wait(timeout_ns)はタイムアウトでfalse、read(timeout_ns)は例外。最後の要求を完了前に破棄すると、安全な解放のため待機する。
- bytesはnative formatの順序・色空間のまま行間paddingなし。rowStrideはバイト単位。BGRA→RGBA、上下反転、色空間変換、画像エンコードは利用側で行う。
- 別queueとのSemaphore/所有権移譲、queueへの並行CPUアクセス、同じ要求への並行read/mapは利用側で同期する。未送信のproducerの結果は取得できない。

## Swapchain、互換性、寿命

Swapchain末尾のusage引数に `ImageType::ColorAttachment | ImageType::TransferSrc` を渡すと、Surfaceの対応usageを確認して作成する。非対応なら例外にし、暗黙に用途を落とさない。その環境では転送可能なオフスクリーン画像へ描画する。

直接Swapchain画像を読む場合は、取得済みの画像を選び、描画送信とpresentの間へ転送を配置する。今回の変更は既存DefaultPhaseのsubmitとpresentの分離を行わない。その経路へのスクショ機能の接続には送信・表示順序の連携が別途必要。

- RenderPass(swapchain)、FrameBuffer(swapchain, pass)、旧RenderPassBegin(pass, framebuffer, color)を維持する。
- IDPickingRenderPass/IDPickingRenderPassBegin/pl::IDPickingは共通実装へ委譲する互換ラッパー。新しい用途では追加不要。
- 新FrameBufferは1対象でBeginのindexは既定0。旧一括FrameBufferでも新BeginのtargetIndexで取得画像を明示できる。旧Beginの自動選択は従来のフレーム番号を維持する。既知のフレーム/取得画像番号の全面整理は今回の範囲外。
- FrameBufferは画像とRenderPassを保持する。借用Swapchain画像はSwapchainとSurfaceを保持する。描画送信後のFrameBuffer/Pipeline等は利用側がGPU完了まで保持する。
- ImageReadbackが保持するのは転送に必要なリソースであり、描画リソース全体のGPU寿命管理を代替しない。
- リサイズは利用側で新画像とFrameBufferを作る。同じformat/sample構成ならRenderPassを再利用できる。旧リソースはGPU利用完了後に解放する。

## 検証対象

公開APIのみで、カラー＋整数ID同時出力、signed/unsigned clear、深度によるfragment棄却、複数subpass、MSAA resolve、配列レイヤー、Loadによる保持、全画面/部分領域読み戻し、layout復帰、元画像の寿命保持を検証する。これらはValidation Layerのerrorを捕捉し、リソース破棄時も含めテスト失敗へ反映する。

既存Render fixtureで行っていた内部RenderPass差し替えを撤去した。修正前に失敗したIDPickingの基底Accessor、深度state、配列画像サイズ、3定数のformatサイズの回帰テストを通常実行へ戻した。

## 2026-10-17 検証結果

Apple M1 / MoltenVK、Apple Clang 17、Vulkan SDK 1.4.309.0、Debugで実行。

- VSL通常実行: 184件成功、失敗0、環境skip 0。既存の未修正不具合26件はDISABLEDのまま。
- うち共通描画・読み戻し9件と実Swapchain読み戻し1件はValidation Layerを有効化し、errorをテスト失敗へ反映。10件とも成功。
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
