# Resourceと矩形アップロードの仮実装

Textureは`AHO/x/resource`のexperimental APIです。Bufferは[既存のAHO Resourceへ統合](resource-buffer.md)しました。msdfgen・atlas allocator・文字描画はまだ追加していません。外部ライブラリの依存追加はありません。

```cpp
#include <AHO/x/command/texture_update.hpp>
#include <VSL/vulkan/phase.hpp>

namespace resource = aho::x::resource;

// ページと転送領域は描画ループの外で作る。
resource::Texture atlas(manager.get_device(), 1024, 1024);
aho::resource::Buffer<aho::resource::BufferType::GPU_FRIENDLY> upload(manager.get_device());
upload.reserve(64 * 64 * 4);

// glyph_rgbaは余白込み、行を詰めた線形RGBA8の画素列。
upload.assign(glyph_rgba);
{
    auto phase = manager.start_phase<vsl::ComputePhase>();
    phase << atlas.update(upload, {.x = 32, .y = 16, .width = 24, .height = 32});
}
// 次の転送元を書き換える前に完了を確認する。
upload.wait();
```

## Resourceの契約

- CPUデータは`aho::resource::Buffer<GENERAL>`、転送元は`Buffer<GPU_FRIENDLY>`を使います。詳細は[Bufferの契約](resource-buffer.md)を参照してください。
- `Texture::update()`はgpu_buffer conceptを受け、GPU_FRIENDLY・ON_GPU・VSL Bufferを包んだCustomを同じ経路で利用できます。必要なTRANSFER_SOURCE usageはVSL側で検証します。
- `Texture::update(Buffer<GENERAL>)`はCPU画素をコマンド記録時にstagingへコピーします。stagingの所有権はGPU完了まで維持され、記録後のCPU Bufferを書き換えても転送内容は変わりません。反復更新では既存のGPU_FRIENDLY Bufferを再利用できます。
- コピーしたResourceは容量・内容・使用状態を共有します。独立した内容が必要なら別のResourceを構築してください。CPUのspanはresize/reserveで無効になる場合があります。
- `Texture`は線形RGBA8（既定）またはsRGB RGBA8、1レイヤー・1ミップのGPUテクスチャです。MSDFの3チャンネルはRGBへ、余ったAは呼び出し側で設定してください。転送時は元のバイト列を保持し、sRGB形式ではサンプリング時にRGBを線形値へ復号します。初回更新時はページ全体をゼロで埋め、その後の更新では他の画素を維持します。

## コマンドの配置

Resourceは`AHO/resource/`、コマンドは`AHO/command/`へ置く。experimental側も同じ構造で`AHO/x/resource/`と`AHO/x/command/`に分ける。実装とテストのディレクトリもこの構造に揃える。

- `aho::command::BufferUpload`：`<AHO/command/buffer_upload.hpp>`
- `aho::x::command::TextureUpdate`：`<AHO/x/command/texture_update.hpp>`

`buffer.upload()`と`texture.update()`の呼び方は維持する。これらを使う側は対応するコマンドヘッダーをincludeする。Resourceヘッダーからコマンドヘッダーを読み戻す循環includeは設けない。

## 記録と同期

`texture.update(upload, region, byte_offset)`はコマンドを作るだけです。streamがinvokeした時点のuploadデータを使います。RenderPassBeginより前など、レンダーパスの外で記録してください。regionの幅・高さが両方0なら画像全体です。転送元は行を詰めたデータで、offsetは4の倍数です。

同じPhaseで同じ転送元Bufferの異なるoffsetから複数の矩形を転送できます。最初の記録以降は書き換えず、先に全データをassignしてください。同じTextureまたは転送元Bufferを別のPhaseへ記録するには、前の更新の完了が必要です。単一Phase内でまとめて更新するか、独立したTextureと転送元Bufferを用意します。自動スロット増設や暗黙の待機は行いません。

`busy()`は記録中・送信中にtrueです。使用中のreserve/assign/writeは例外になります。`wait()`は送信後に待機し、未送信の記録にはfalseを返します。キャンセル済み・使用履歴なしはtrueです。初回更新をキャンセルしても、次回は初期化からやり直せます。

Phase側はBufferのResource（Storage・Customization・マップ）と画像の所有者を保持します。AHOのResourceがCompletionを保持しても循環参照になりません。AHO側のラッパーを記録後に解放してもGPU処理に必要な所有者を維持します。

`image()`は描画側へのVSL ImageAccessorです。最初の更新が送信される前はサンプリングできません。公開アクセサーを使って画像レイアウトを外部で変更する場合はSHADER_READ_ONLY_OPTIMALへ戻してください。他のqueueを使う同期、並行したCPUからの操作、descriptorの更新タイミングは呼び出し側の責任です。

## VSLの追加

- `BufferAccessor::get_device/get_usage/get_memory_properties()`：公開APIによるBufferのメタデータ取得。
- `CopyBuffer`：所有権を保持する非同期の範囲転送コマンド。
- `CopyBufferToImage`：矩形・レイヤー・転送元offset・Bufferの所有権に対応。
- `ImageUpload`：初回clear、転送前後のbarrier、矩形コピーを記録。
- `CommandBuffer::completion()`：現在記録している送信の完了状態を取得。

Vulkan固有の操作はVSLに閉じ込めています。AHOの実装をVulkan識別子禁止でコンパイルする契約テストも追加しています。

複数upload slot、RGBA8以外の画像形式、atlasの矩形配置は未対応です。

矩形・バッファoffsetの定義は[VulkanのVkBufferImageCopy](https://docs.vulkan.org/refpages/latest/refpages/source/VkBufferImageCopy.html)に対応します。
