# ReadbackのVSL / AHO境界

RenderPass構成のPR #56、PhaseのPR #105に続く変更。AHOのImageReadbackは公開VSL APIだけで構成し、Vulkan API、VSLの_pimpls.hpp、内部device / queue / Buffer_implを操作しない。

## 公開VSL API

- `Buffer<usage, MemoryProperty::HostVisible>(device, bytes)`: 既存Bufferを使う。HostCoherentを優先し、非coherentでも利用できる。
- `Buffer::data()`: 割当全体を初回だけmapし、Bufferの共有ストレージが破棄されるまで維持する。複数のholderやBufferのコピーはmapを共有する。holder自身もストレージを保持する。
- `LocalBufferHolder::read(bytes, offset)` / `write(bytes, offset)`: holder内の相対範囲を検証し、readはinvalidate、writeはコピー後flushを行う。非coherentの範囲はatom境界へ拡張する。GPU完了待ちは呼び出し側の責務。spanの利用中はholderまたはBufferを保持する。
- `LocalBufferHolder::flush()` / `flush(bytes, offset)` / `invalidate(bytes, offset)`: dataポインタへ直接アクセスした場合のキャッシュ操作。長寿命holderではGPUによる更新後にもreadまたはinvalidateを呼ぶ。
- `BufferAccessor::requestedSize()`: 要求した論理サイズ。従来の`size()`は割当サイズを返す契約を維持する。
- `CommandManager::independent(count)`: 同じdevice / queueを使う独立したpoolとCommandBuffer群を作る。元managerで記録中のCommandBufferをresetしない。
- `CommandManager::getDevice()` / `ImageAccessor::getDevice()`: 公開Accessor同士でdeviceの整合性を確認できる。
- `command::ImageReadback(&buffer, image, layout, region)`: 永続mapした転送先を利用する。TransferDestination usageを確認する。Phase内で記録すると、転送先Bufferと元画像を、その送信のCompletionに保持する。

```cpp
vsl::Buffer<vsl::MemoryType::TransferDestination, vsl::MemoryProperty::HostVisible> destination(device, byteSize);
auto mapped = destination.data();
auto transfer = producer.independent();
auto phase = transfer.startPhase<vsl::ComputePhase>();
phase << vsl::command::ImageReadback(&destination, image, layout, region);
auto completed = phase.completion();
phase.submit();
if (completed.wait()) {
    auto bytes = mapped.read(byteSize);
    // bytesの利用中はmappedまたはdestinationを保持する。
}
```

## AHO側に残す責務

AHO ImageReadbackは上記を組み合わせ、必要容量の判断、オブジェクトの保持と再利用、busy判定、CPU側ImagePixelsへのコピーを担当する。capture / ready / wait / read / capacityの既存契約を維持する。

同時要求数はmaxRequestsで指定する（既定1）。チケット保持中またはGPU使用中のスロットは再利用しない。従来captureは完了前ならfalseを返して最新結果を維持する。captureTicketは専用Phaseを送信し、recordは呼び出し側のPhaseへ記録する。Bufferは必要容量が増えたときだけ交換し、構築に失敗した場合は以前のBufferを保持する。read(destination)はCPU vectorの容量も再利用する。

AHOLiの製品targetからVulkan SDKの直接include設定を削除した。Vulkanのfixtureを使うGPUテストには、SDKヘッダーの順序をテストtargetで明示する。

## 検証方針

- AHO実装のcompile契約で、Clang / GCCのpoison pragmaによりnative Vulkan識別子を使用禁止にする。移行前は失敗し、移行後は成功する。MSVCでは同じsourceのcompileのみ実施する。
- 実GPUで繰り返しコピー後のmapping addressの一致、部分範囲、サイズ境界、alias、記録された転送先の寿命、独立managerの非干渉を検証する。
- non-coherentのinvalidate範囲はValidation Layer付きで検証する。coherent memory上で明示的にinvalidate経路を通すケースは、物理的なnon-coherent heapの動作を再現したという意味ではない。
- 既存のAHO Readback 13ケースによる画像内容・region/layer・再利用・容量拡張・busyを回帰検証する。

MappedBufferは既存BufferとLocalBufferHolderへ統合した。旧MappedBufferの利用者は、用途を指定したHostVisible Bufferを保持し、data()で取得したholderのread/writeへ移行する。転送コマンドは構築時のBufferストレージを保持するため、元のBufferオブジェクトの破棄や差し替えに影響されない。同じBufferへのCPU/GPUアクセスは呼び出し側で同期する。

## 2026-10-18 検証結果

Apple M1 / MoltenVK、Vulkan SDK 1.4.309.0、Debugで関連targetのビルド成功。VSL全体、AHO Readback / DrawPhase、AHOヘッダーと依存境界のcompile契約で213件成功、失敗0、環境skip 0。既存26件はDISABLED。新MappedBufferの4件はValidation Layer付き。関連4公開ヘッダーの単独compileとVisual Studio project / filtersの登録も確認した。Windows / Linux実行、物理的なnon-coherent heapでの検証は未実施。

25 accepted capturesで `buffers=1 pools=2 allocate_command_buffers=2 fences=2 memories=2 maps=1` を観測した。pool / buffer群 / Fenceの各2はproducerとreaderの各1組であり、毎回の再生成ではない。CPU側vector storage再利用も既存ケースで成功。時刻による性能比較はこの変更では再計測していない。

```sh
ctest --test-dir build-vsl-boundary --output-on-failure \
  -R '^(VSL\.|AHO\.(ImageReadback(Busy)?VulkanIntegrationTest|DrawPhaseVulkanIntegration)\.|AHO\.Compile\.(Header\.image_readback\.hpp|ImageReadback\.PublicVslOnly)$)'
```

## 2026-10-20 セルフレビュー

MappedBufferの確保失敗時の解放、論理サイズの範囲検証、invalidateのatom境界と割当末尾、map所有者の保持、独立managerの非干渉、AHO側の公開API境界を確認した。このPR固有の追加修正が必要な問題は見つからなかった。親の#105で修正したSemaphore保持と外部Fence再利用時のCompletion保存を取り込んだ。
