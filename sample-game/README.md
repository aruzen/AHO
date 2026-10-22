# WANDER — AHO/VSL歩行サンプル

`visual-image/` のローポリ森林、雪山、湖、オーロラを参考にした一人称の散策サンプルです。湖畔・雪の森・枯れ木の森は同じ地形上につながっています。戦闘やクリア条件はありません。

ゲームを通じてエンジンの不足を確認することが目的です。[実装順序と不足機能](../documants/sample-game-engine-order.md)に、実装前の問題・対応・次に必要な機能を記録しています。

## 起動

リポジトリルートで次を実行します。

```sh
./sample-game/run.sh
```

CMake 3.30以降、C++23対応コンパイラ、Vulkan SDK、GLFW、Boost、GLM、GTest、glslangValidatorが必要です。このMacではHomebrew LLVMと `$HOME/VulkanSDK` を自動検出します。`VULKAN_SDK`、`AHO_WANDER_BUILD_DIR`、`AHO_BUILD_JOBS`で上書きできます。

初回構成時のみ、stbとtinygltfの固定バージョンを取得します。オフライン時はCMakeの `AHO_STB_INCLUDE_DIR` と `AHO_TINYGLTF_INCLUDE_DIR` を指定してください。Foxモデルは取り込んだglTF PRの同梱素材を使い、実行時のダウンロードはありません。

macOSでは、SDK同梱MoltenVK 1.2.11で再現したリサイズ後の1×1描画不具合を避けるため、公式MoltenVK 1.4.2（約57 MiB、SHA256固定）をビルドディレクトリ内に取得します。サンプルのプロセス内だけでこのdriverを選び、システムや既存SDKは変更しません。[公式の修正履歴](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2)に同症状の修正があります。

オフラインでは `AHO_WANDER_ICD_FILE=/path/to/MoltenVK_icd.json`（1.4.2以降）を指定できます。既存driverを検証する場合は `AHO_WANDER_USE_SYSTEM_MOLTENVK=1` を設定するか、CMakeで `AHO_WANDER_LOCAL_MOLTENVK=OFF` にしてください。1.4.2のmacOS最低要件は12です。

ビルド済みなら `build/wander/sample-game/aho-wander` を直接実行できます。shader・modelはビルド時に絶対パスを設定するため、起動ディレクトリに依存しません。

## 操作

| 操作 | キー |
|---|---|
| 歩く／走る | WASD／Shift |
| 視点操作 | 画面クリック後にマウス、または矢印キー |
| マウスを解放 | Esc |
| 湖畔／雪の森／夜の森へ移動 | 1／2／3 |
| 昼夜をなめらかに切り替え | T |
| 操作表示を隠す | H |
| PNG撮影 | F12（`wander-captures/`へ保存） |
| 終了 | Q、またはウィンドウを閉じる |

木の幹・岩・岸・外周に衝突します。視点は描画と同じ地形三角形の高さに追従します。フォーカスを失うとマウス捕捉を解除し、復帰時の視点の飛びを抑止します。

## 検証

```sh
./sample-game/run.sh --self-test
./sample-game/run.sh --verify build/wander/captures
./sample-game/run.sh --view 3 --frames 120
```

`--self-test` はGPUを作らず、1800ステップの歩行、岸・障害物・外周、頂点の有限性を検査します。`--verify` は実ウィンドウとVulkan validationを使い、移動、複数景観、昼夜、アニメーション、frame slot再利用、リサイズを検証してPNGを保存します。`--frames` は通常ループの終了フレーム数です。

## 使用しているエンジン機能

- AHO: `StandardEngine`、`Window`、`InputManager`、`Key`、`Cursor`、`MouseButton`、`DrawPhase`、`x::Screenshot`。
- AHO experimental: `x::scene::Camera`、`RenderTarget`、`MeshRenderer`、`GltfRenderer`、`x::resource::GltfModel`。ゲーム側からGLFWや生のVulkan APIを直接呼びません。
- VSL: GPU buffer/image/sampler、descriptor、pipeline、depth attachment、render pass、framebuffer、command、GPU完了通知。glTF関節paletteはframe slot単位で更新します。
- サンプル固有: 地形と木々の生成、簡易衝突と歩行、昼夜・霧・水面・オーロラ・操作表示shader。地形のベクトル演算にはGLMを使い、cameraの行列はAHOの型で作ります。

`RenderTarget` がWindowのresize pluginへ登録され、深度画像とframebufferの再構築後に両rendererへ通知します。Screenshotのusage変更も同じ通知経路を通り、毎フレームの `prepare()` は不要です。描画は `target.begin(phase)` → `renderer.record(...)` → `foxRenderer.record(...)` の順です。RenderTargetは両rendererより先に作成し、後で破棄します。

## 素材

環境の形状とshaderはコードで生成します。FoxはPR #201/#206から取り込んだKhronos glTF-Sample-Assetsのモデルです。モデル: PixelMannen（CC0）、Rigging & Animation: tomkranis（CC-BY-4.0）、glTF変換: @AsoboStudio / @scurest（CC-BY-4.0）。出典・ライセンスは [examples/assets/fox](../examples/assets/fox/LICENSE.md) に保持しています。
