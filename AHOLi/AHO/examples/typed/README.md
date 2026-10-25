# Typed graphics sample

2つの色付きquadを異なる向きに回転させる、ウィンドウ付きのSampleです。Typed本体に新しい機能を追加せず、頂点入力・UBO・push constant・16bit index・頂点view・batchを組み合わせています。

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

C++23対応のcompilerを使います。macOSの検証はLLVM 20/libc++で行いました。必要に応じてconfigureへ`-DCMAKE_CXX_COMPILER=/path/to/clang++`を追加します。Vulkan SDKの環境設定を読み込み、Vulkan driverとvalidation layerを利用できる状態で実行してください。shaderはbuild時にSPIR-Vへ変換し、実行時のカレントディレクトリには依存しません。

閉じるまで描画を続けます。フレーム数を指定して終了する場合は次のように実行します。

```sh
./build/typed-example/bin/aho-typed-example --frames 120
```

## 画像による検証

デスクトップ上で実ウィンドウを開き、通常・回転後・resize後の3枚を保存します。

```sh
./build/typed-example/bin/aho-typed-example --verify build/typed-example/captures
```

左右それぞれに色付きquadが描かれたこと、push値を変えると画素が変わること、resize後の画像サイズが変わることを検査します。同じattachment形式ならpipelineの生成回数が増えないことも確認します。

## Typedの使いどころ

- `VertexRecord` → `TypedVertexInput`でC++の頂点structからattributeとbindingを宣言します。
- `BlockRecord` → `UniformBinding` / `PushRange`でshaderの配置とC++の型を結びます。
- `TypedUniformBuffer`を`TypedDescriptorSet::write<0>`へ渡します。UBOの色係数は固定し、実行中の書き換えは行いません。
- `TypedVertexBuffer::view(1, 4)`で先頭の不要レコードを除外します。
- `TypedIndexBuffer<uint16_t>`を使ってquadの2三角形を描きます。
- `Pushes::Values`へ各quadの変換をコピーし、`TypedBatch`で2描画をまとめます。同じpipeline・vertex/index buffer・descriptorへのbindは各1回です。

window、swapchain、送信、resize通知、Screenshotは既存AHO/VSLのAPIを使います。詳細と残る課題は[documants/typed-example.md](../../../../documants/typed-example.md)に記録しています。
