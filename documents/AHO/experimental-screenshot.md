# experimentalスクリーンショット

`<AHO/x/screenshot.hpp>`の`aho::x::Screenshot`で、AHOの描画フレームをPNGへ保存する。

```cpp
#include <AHO/x/screenshot.hpp>
#include <AHO/drawer/draw_phase.hpp>

aho::x::Screenshot screenshot(engine.getWindow());
{
    aho::DrawPhase phase(engine);
    // 通常の描画コマンドを記録する。
    screenshot.capture(phase);
}
screenshot.save("screenshot.png");
```

`Screenshot`は描画ループの外で1つ作成し、撮影のたびに再利用する。最初の作成時にウィンドウの画像を読み戻せる構成へ変更する。描画中の作成はエラーになる。通常のリサイズでも読み戻し用の設定を維持する。

`capture(phase)`は、そのPhaseの終了コールバックを維持したまま、終了コールバックの後に読み戻しを追加する。既定の終了処理も自動で行う。撮影対象はそのPhaseで取得した画像であり、描画と読み戻しを1回の送信で実行する。

前の撮影が記録待ち・転送中なら`capture()`は`false`を返し、進行中の要求を維持する。`true`を返すと以前の結果を手放し、新しいフレームを撮影する。コピーした`Screenshot`は撮影状態と結果を共有する。

`capture()`の呼び出し後は`phase.finish`を置き換えない。独自の終了コールバックを使う場合は、通常のDrawPhaseと同じく、そのコールバックでRenderPassEndまで記録する。明示的な`submit()`や`submitCommands()`にも対応する。

`save(path)`と`read()`はPhaseの送信後に呼ぶ。GPUの完了待ちはここで行う。記録待ちやキャンセル済みのフレーム、撮影前の読み取りは例外で通知する。描画スレッドで使用し、同じオブジェクトへの並行呼び出しは行わない。

描画ループを止めずに結果を回収する場合は、`ready()`を確認してから保存できる。

```cpp
if (screenshot.ready()) {
    screenshot.save("screenshot.png", 0);
}
```

PNGのエンコードとファイル書き込みは同期処理。HDR・整数ID画像は対応せず、RGBA8/BGRA8のUNORM・sRGBに対応する。BGRAはRGBAへ並べ替え、行順とアルファ値を維持する。追加のガンマ変換や上下反転は行わない。読み戻し可能なSwapchain画像を用意できない環境では、作成時に例外となる。

取得済みの`aho::ImagePixels`も、同じPNG保存処理へ渡せる。

```cpp
aho::x::saveScreenshot(pixels, "offscreen.png");
```

保存先の親ディレクトリは事前に用意する。既存ファイルは上書きする。非対応形式・サイズ・データ不足は`std::invalid_argument`、エンコード・ファイル保存の失敗は`std::runtime_error`で通知する。

配置はヘッダーが`include/AHO/x/screenshot.hpp`、実装が`src/x/screenshot.cpp`、テストが`test/x/screenshot_test.cpp`。PNGエンコーダーには、既存の`thirdparty/stb_image_write.h`を使用する。

## 検証

Apple M1 / MoltenVK・Vulkan SDK 1.4.309.0・Debugで、関連53件が成功した。内訳は既存のAHO GPU 39件、スクリーンショットのGPU 5件、PNGのCPU 3件、公開ヘッダーとVulkan直接依存禁止のコンパイル契約4件、一時ファイルの準備・後片付け2件。失敗・環境skipは0。

保存したPNGを既存のstbデコーダーで再度読み、4形式の色・アルファ・行順・パディングと日本語ファイル名を確認した。不正な入力で既存ファイルを破壊しないこと、保存先エラーも確認した。実GPUでは予約中の重複拒否、明示送信、独自の終了処理、リサイズ、キャンセル・例外後の再撮影、別ウィンドウの拒否を検証した。

Windows/Linuxの実行と、AHO/VSLの全テストは今回の検証範囲外。
