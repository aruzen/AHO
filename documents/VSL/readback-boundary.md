# ReadbackのVSL / AHO境界

RenderPass構成のPR #56、PhaseのPR #105に続く変更。AHOのImageReadbackは公開VSL APIだけで構成し、Vulkan API、VSLの_pimpls.hpp、内部device / queue / Buffer_implを操作しない。

## 公開VSL API

- `MappedBuffer(device, bytes, usage)`: host-visible Bufferを確保して一度mapする。コピーは同じbufferとmapを共有する。HostCoherentを優先し、非coherentでも利用できる。
- `MappedBuffer::read(bytes, offset)`: 要求範囲を検証し、非coherent時はatom境界に合わせてinvalidateしてから読み取り専用spanを返す。submitやGPU待機は行わない。呼び出し側がGPU writerの完了を待ち、span使用中はMappedBufferを保持する。
- `CommandManager::independent(count)`: 同じdevice / queueを使う独立したpoolとCommandBuffer群を作る。元managerで記録中のCommandBufferをresetしない。
- `CommandManager::getDevice()` / `ImageAccessor::getDevice()`: 公開Accessor同士でdeviceの整合性を確認できる。
- `command::ImageReadback(mappedBuffer, image, layout, region)`: 永続mapした転送先を利用する。TransferDestination usageを確認する。Phase内で記録すると、転送先のmapを含む所有者と元画像を、その送信のCompletionに保持する。

```cpp
vsl::MappedBuffer destination(device, byteSize);
auto transfer = producer.independent();
auto phase = transfer.startPhase<vsl::ComputePhase>();
phase << vsl::command::ImageReadback(destination, image, layout, region);
auto completed = phase.completion();
phase.submit();
if (completed.wait()) {
    auto bytes = destination.read(byteSize);
    // bytesを利用する。destinationの寿命を維持する。
}
```

## AHO側に残す責務

AHO ImageReadbackは上記を組み合わせ、必要容量の判断、オブジェクトの保持と再利用、busy判定、CPU側ImagePixelsへのコピーを担当する。capture / ready / wait / read / capacityの既存契約を維持する。

同時進行の転送は1件。受理済みcaptureごとに追加submitを1回行い、完了前のcaptureはfalseを返して結果を維持する。Bufferは必要容量が増えたときだけ交換し、構築に失敗した場合は以前のBufferを保持する。read(destination)はCPU vectorの容量も再利用する。

AHOLiの製品targetからVulkan SDKの直接include設定を削除した。Vulkanのfixtureを使うGPUテストには、SDKヘッダーの順序をテストtargetで明示する。

## 検証方針

- AHO実装のcompile契約で、Clang / GCCのpoison pragmaによりnative Vulkan識別子を使用禁止にする。移行前は失敗し、移行後は成功する。MSVCでは同じsourceのcompileのみ実施する。
- 実GPUで繰り返しコピー後のmapping addressの一致、部分範囲、サイズ境界、alias、記録された転送先の寿命、独立managerの非干渉を検証する。
- non-coherentのinvalidate範囲はValidation Layer付きで検証する。coherent memory上で明示的にinvalidate経路を通すケースは、物理的なnon-coherent heapの動作を再現したという意味ではない。
- 既存のAHO Readback 13ケースによる画像内容・region/layer・再利用・容量拡張・busyを回帰検証する。

既存Bufferの局所修正PR #78〜#80やBufferAccessorの多態破棄PR #103の差分は取り込まない。新しいMappedBufferはBufferAccessorを継承しないため、未修正のLocalBufferHolderのmap / unmap処理には依存しない。

## 2026-10-18 検証結果

Apple M1 / MoltenVK、Vulkan SDK 1.4.309.0、Debugで関連targetのビルド成功。VSL全体、AHO Readback / DrawPhase、AHOヘッダーと依存境界のcompile契約で213件成功、失敗0、環境skip 0。既存26件はDISABLED。新MappedBufferの4件はValidation Layer付き。関連4公開ヘッダーの単独compileとVisual Studio project / filtersの登録も確認した。Windows / Linux実行、物理的なnon-coherent heapでの検証は未実施。

25 accepted capturesで `buffers=1 pools=2 allocate_command_buffers=2 fences=2 memories=2 maps=1` を観測した。pool / buffer群 / Fenceの各2はproducerとreaderの各1組であり、毎回の再生成ではない。CPU側vector storage再利用も既存ケースで成功。時刻による性能比較はこの変更では再計測していない。

```sh
ctest --test-dir build-vsl-boundary --output-on-failure \
  -R '^(VSL\.|AHO\.(ImageReadback(Busy)?VulkanIntegrationTest|DrawPhaseVulkanIntegration)\.|AHO\.Compile\.(Header\.image_readback\.hpp|ImageReadback\.PublicVslOnly)$)'
```

## 2026-10-20 セルフレビュー

MappedBufferの確保失敗時の解放、論理サイズの範囲検証、invalidateのatom境界と割当末尾、map所有者の保持、独立managerの非干渉、AHO側の公開API境界を確認した。このPR固有の追加修正が必要な問題は見つからなかった。親の#105で修正したSemaphore保持と外部Fence再利用時のCompletion保存を取り込んだ。
