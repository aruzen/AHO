# Typed graphics sample

2つの色付きquadを異なる向きに回転させ、カメラと色も動かす、ウィンドウ付きのSampleです。AHO BufferをTyped viewで接続し、頂点入力・フレームごとのUBO／descriptor・push constant・16bit index・頂点view・batchを組み合わせています。

まず[scene.hpp](scene.hpp)の型宣言、次に[scene.cpp](scene.cpp)の生成と`record()`を読むと描画経路を追えます。[main.cpp](main.cpp)は起動・イベントループ・画像による検証です。

## ビルドと実行

リポジトリルートで実行します。通常のAHO依存に加え、`glslc`が必要です。`thirdparty/stb_image.h` / `thirdparty/stb_image_write.h`は[thirdparty/setup.md](../../../../thirdparty/setup.md)に従って配置してください。git管理外なので別worktreeにも配置が必要です。CMakeは不足時にconfigureで案内します。

```sh
cmake -S . -B build/typed-example \
  -DAHO_HOME="$PWD/AHOLi/AHO" \
  -DVSL_HOME="$PWD/VSLi/VSL" \
  -DAHO_BUILD_TYPED_EXAMPLE=ON \
  -DBUILD_TESTING=OFF
cmake --build build/typed-example --target aho-typed-example -j4
./build/typed-example/bin/aho-typed-example
```

C++26対応のcompilerを使います。macOSの検証はLLVM 20/libc++で行いました。必要に応じてconfigureへ`-DCMAKE_CXX_COMPILER=/path/to/clang++`を追加します。Vulkan SDKの環境設定を読み込み、Vulkan driverとvalidation layerを利用できる状態で実行してください。shaderはbuild時にSPIR-Vへ変換し、実行時のカレントディレクトリには依存しません。

閉じるまで描画を続けます。フレーム数を指定して終了する場合は次のように実行します。

```sh
./build/typed-example/bin/aho-typed-example --frames 120
```

## 画像による検証

デスクトップ上で実ウィンドウを開き、通常・回転後・resize後の3枚を保存します。

```sh
./build/typed-example/bin/aho-typed-example --verify build/typed-example/captures
```

まず画像の読み戻しを待たずにフレーム枠の4周分を送信します。その後、左右それぞれに色付きquadが描かれたこと、毎フレームのUBO更新で画素が変わること、resize後の画像サイズが変わることを検査します。同じattachment形式ならpipelineの生成回数が増えないことも確認します。

## Typedの使いどころ

- `VertexRecord` → `TypedVertexInput`でC++の頂点structからattributeとbindingを宣言します。
- `BlockRecord` → `UniformBinding` / `PushRange`でshaderの配置とC++の型を結びます。
- `SceneFrame`へAHOの`Buffer<GPU_FRIENDLY>`と`TypedDescriptorSet`をまとめ、`TypedFrameResources<SceneFrame>`でcommand slotごとに独立して生成します。`uniform_view<UniformLayout>(buffer)`でUBOのpayload型を渡します。
- `frames.update(phase, callback)`で現在の枠のUBOだけを更新し、`frames.get(phase)`で描画に使います。色係数・カメラ位置・aspect比・時刻を毎フレーム渡します。GPU完了待ちは既存Phaseが担当します。
- `vertex_view<Vertex>(buffer, 1, 4)`で先頭の不要レコードを除外します。draw／dispatchの記録後はAHO Bufferのbusyへ反映され、完了前のwrite／再確保を拒否します。
- `TypedIndexBuffer<uint16_t>`を使ってquadの2三角形を描きます。
- `Pushes::Values`へ各quadの位置・scale・回転速度をコピーし、`TypedBatch`で2描画をまとめます。同じpipeline・vertex/index buffer・descriptorへのbindは各1回です。画像の時間変化はUBOが担当するため、検証でUBO更新の効果を確認できます。

device・allocator・render pass・format・viewport/scissorは`RenderContext`から取得します。window、送信、resize通知、Screenshotは既存AHO/VSLのAPIを使います。詳細は[documants/typed-frame-resources.md](../../../../documants/typed-frame-resources.md)と[documants/typed-example.md](../../../../documants/typed-example.md)に記録しています。
