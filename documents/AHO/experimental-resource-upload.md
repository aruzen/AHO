# Resourceと矩形アップロードの仮実装

Textureは`AHO/x/resource`のexperimental APIです。Bufferは[既存のAHO Resourceへ統合](resource-buffer.md)しました。msdfgen・atlas allocator・文字描画はまだ追加していません。外部ライブラリの依存追加はありません。

```cpp
#include <AHO/x/resource/texture.hpp>

namespace resource = aho::x::resource;

// ページと転送領域は描画ループの外で作る。
resource::Texture atlas(manager.getDevice(), 1024, 1024);
aho::resource::Buffer<aho::resource::BufferType::GPUFriendly> upload(manager.getDevice());
upload.reserve(64 * 64 * 4);

// glyphRgbaは余白込み、行を詰めた線形RGBA8の画素列。
upload.assign(glyphRgba);
{
    auto phase = manager.startPhase<vsl::ComputePhase>();
    phase << atlas.update(upload, {.x = 32, .y = 16, .width = 24, .height = 32});
}
// 次の転送元を書き換える前に完了を確認する。
upload.wait();
```

## Resourceの契約

- CPUデータは`aho::resource::Buffer<General>`、転送元は`Buffer<GPUFriendly>`を使います。詳細は[Bufferの契約](resource-buffer.md)を参照してください。
- `Texture::update()`はGpuBuffer conceptを受け、GPUFriendly・OnGPU・VSL Bufferを包んだCustomを同じ経路で利用できます。必要なTransferSource usageはVSL側で検証します。
- コピーしたResourceは容量・内容・使用状態を共有します。独立した内容が必要なら別のResourceを構築してください。CPUのspanはresize/reserveで無効になる場合があります。
- `Texture`は線形RGBA8、1レイヤー・1ミップのGPUテクスチャです。MSDFの3チャンネルはRGBへ、余ったAは呼び出し側で設定してください。色空間変換は行いません。初回更新時はページ全体をゼロで埋め、その後の更新では他の画素を維持します。

## 記録と同期

`texture.update(upload, region, byteOffset)`はコマンドを作るだけです。streamがinvokeした時点のuploadデータを使います。RenderPassBeginより前など、レンダーパスの外で記録してください。regionの幅・高さが両方0なら画像全体です。転送元は行を詰めたデータで、offsetは4の倍数です。

同じPhaseで同じ転送元Bufferの異なるoffsetから複数の矩形を転送できます。最初の記録以降は書き換えず、先に全データをassignしてください。同じTextureまたは転送元Bufferを別のPhaseへ記録するには、前の更新の完了が必要です。単一Phase内でまとめて更新するか、独立したTextureと転送元Bufferを用意します。自動スロット増設や暗黙の待機は行いません。

`busy()`は記録中・送信中にtrueです。使用中のreserve/assign/writeは例外になります。`wait()`は送信後に待機し、未送信の記録にはfalseを返します。キャンセル済み・使用履歴なしはtrueです。初回更新をキャンセルしても、次回は初期化からやり直せます。

Phase側はBufferのResource（Storage・Customization・マップ）と画像の所有者を保持します。AHOのResourceがCompletionを保持しても循環参照になりません。AHO側のラッパーを記録後に解放してもGPU処理に必要な所有者を維持します。

`image()`は描画側へのVSL ImageAccessorです。最初の更新が送信される前はサンプリングできません。公開アクセサーを使って画像レイアウトを外部で変更する場合はShaderReadOnlyOptimalへ戻してください。他のqueueを使う同期、並行したCPUからの操作、descriptorの更新タイミングは呼び出し側の責任です。

## VSLの追加

- `BufferAccessor::getDevice/getUsage/getMemoryProperties()`：公開APIによるBufferのメタデータ取得。
- `CopyBuffer`：所有権を保持する非同期の範囲転送コマンド。
- `CopyBufferToImage`：矩形・レイヤー・転送元offset・Bufferの所有権に対応。
- `ImageUpload`：初回clear、転送前後のbarrier、矩形コピーを記録。
- `CommandBuffer::completion()`：現在記録している送信の完了状態を取得。

Vulkan固有の操作はVSLに閉じ込めています。AHOの実装をVulkan識別子禁止でコンパイルする契約テストも追加しています。

複数upload slot、画像format追加、atlasの矩形配置は今回の範囲外です。

矩形・バッファoffsetの定義は[VulkanのVkBufferImageCopy](https://docs.vulkan.org/refpages/latest/refpages/source/VkBufferImageCopy.html)に対応します。
