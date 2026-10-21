# glTFアニメーション表示サンプル

`feat/aho-gltf-assets`（PR #201）から分岐した、`examples/gltf_viewer.cpp`の画面表示サンプル。
同梱のFoxをtinygltf経由で読み、脚・胴体・尻尾が動くRun（走行）の骨格アニメーションをウィンドウで再生する。

## 起動

AHO/VSLの既存依存とglslcを用意する。tinygltfは親実装のCMake設定により固定版を取得する。
AHO_HOME/VSL_HOME未設定の場合は、それぞれ`AHOLi/AHO`と`VSLi/VSL`の絶対パスもCMakeへ渡す。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DAHO_BUILD_GLTF_VIEWER=ON
cmake --build build --target aho-gltf-viewer
./build/AHOLi/aho-gltf-viewer
```

Spaceで一時停止/再開、Escまたはウィンドウを閉じて終了する。ウィンドウのサイズ変更にも対応。
素材のダウンロードは不要。同梱のglTF・buffer・PNGを使うため、起動時にネット接続は行わない。
shaderはCMakeのcustom commandでコンパイルし、実行ディレクトリに依存せずに読み込む。

```sh
# 特定時刻で停止して表示し、120フレーム後に終了
./build/AHOLi/aho-gltf-viewer --time 0.5 --frames 120
# 歩く動き、または周囲を見回す動きに変更
./build/AHOLi/aho-gltf-viewer --animation Walk
./build/AHOLi/aho-gltf-viewer --animation Survey
# 別の互換モデルを指定
./build/AHOLi/aho-gltf-viewer --model /path/to/model.glb
# 実画面の2時刻とリサイズ後のPNGを保存して検証し、終了
./build/AHOLi/aho-gltf-viewer --verify build/viewer-captures
# 同じ検証をCTestから実行
ctest --test-dir build --output-on-failure -R '^AHO.GltfViewer.WindowIntegration$'
```

`--animation`はクリップ名を完全一致で指定する。省略時はRunがあればRun、それ以外は先頭クリップを使う。
`--frames 0`（既定）は終了フレーム数の制限なし。`--time`はモデル内の秒単位の時刻で、Spaceによりその時刻から再開できる。

## 描画経路

`GltfModel::load` → 選択sceneのnode探索 → vertex/index/画像をVSLへupload →
`sample(animation, time)`で姿勢を評価 → node world matrixと`skinMatrices`のpaletteを渡す → GPUで4関節のlinear blend skinning → `DrawPhase`で描画。
カメラはrest poseの境界から配置する。glTFのcolumn-major/右手系を維持し、射影でVulkan用にYを1回反転する。

baseColor画像はsRGB形式でupload。material/vertex colorと簡単な拡散照明を適用する。
swapchain画像ごとのdepth attachmentを作成し、リサイズ時はGPU完了後にframebuffer/pipelineを再作成する。
vertex/index/textureは初期化時のみuploadし、毎フレームは姿勢行列を更新する。
関節paletteのstorage bufferとdescriptor setはcommand slotごとに用意し、DrawPhaseがそのslotの完了を待ってから書き換える。
法線のないモデルでは面積で重み付けした頂点法線を生成する。skinとworldを合成した行列の逆転置で法線を変換する。

このサンプルは不透明なtriangle、nodeのTRS animation、JOINTS_0/WEIGHTS_0による4関節までのskinningを対象にする。morph、透過、完全なPBR、mipmap生成、素材別の全sampler設定には未対応。
別モデルでも同じ範囲に対応する。nodeの回転だけをプログラムで追加する処理はなく、同梱glTFに記録されたanimationを再生する。

## 素材と利用条件

- Model: Fox。モデル: PixelMannen（2014、CC0-1.0）。
- Rigging & Animation: tomkranis（2014、CC-BY-4.0）。
- glTF変換: @AsoboStudio and @scurest（2017、CC-BY-4.0）。
- [Khronos glTF-Sample-Assetsの固定コミット](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/15cd02d304e54cb42a4db4eefe719f76190f4b68/Models/Fox)。
- `examples/assets/fox/`にGLB、LICENSE.md、metadata.jsonを無改変で同梱。GLB内にtextureとRun/Walk/Surveyの3クリップを含む。
- 以前のAnimatedCubeも`examples/assets/animated_cube/`に保持。作者Norbert Nopper / UX3D（2017）、モデルCC0-1.0、metadata CC-BY-4.0。node animationの確認に利用できる。

## 検証方法

`--verify`は異なる2時刻を描画し、画面のforeground面積・色の種類・画素差分を検査する。
連続送信で全command slotを4周再利用した後、同じ時刻の画素が最初と一致することも確認する。
`frame-0.png`、`frame-1.png`、`frame-repeat.png`、`resized.png`を保存する。描画中のVulkan validationエラーは終了コード1にする。
GPU/表示環境が必要。CPUの読込テストだけでは画面表示済みとは扱わない。
