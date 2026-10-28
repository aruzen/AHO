# Typedによる標準Triangle描画

`aho::draw` は2次元のTriangleと線形RGBAを受け取り、Typedの描画commandを現在の `DrawPhase` へ記録します。

```cpp
#include <AHO/drawer/standard_drawer.hpp>
#include <AHO/drawer/draw_phase.hpp>

aho::Triangle<float, vsl::D2> triangle({-.7f, -.6f}, {.7f, -.6f}, {0, .7f});
aho::DrawPhase phase(engine);
aho::draw(triangle, aho::RGBA(.2f, .7f, 1, 1));
```

明示的なPhase指定と、既存のtuple形式も使えます。

```cpp
aho::draw(triangle, color, phase);
phase << aho::pack(triangle, color);
```

## 座標と描画状態

- 座標はNDCです。xとyの表示範囲は通常 `[-1, 1]`、z=0、w=1です。正の高さを持つVulkan viewportを使用するため、画面上のyは下向きです。画像寸法による縦横比補正やカメラ変換は加えません。
- Windowの標準render pass、subpass 0、単一color attachmentを使用します。viewport／scissorは毎回Window全体へ設定します。深度判定と裏面カリングは無効で、両方の頂点巻き順を表示できます。
- 色は線形のstraight alphaです。RGBは `src * src_alpha + dst * (1 - src_alpha)`、alphaは `src_alpha + dst_alpha * (1 - src_alpha)` で合成します。sRGB attachmentへの変換はGPUが行います。
- 直接呼び出しとtupleの順序が描画順です。頂点と色は呼出し時にpush constantへコピーされ、後から元の変数を変更しても記録済みの描画は変わりません。
- カスタムrender passへの切替えやsubpass移動、RenderPassEnd後の描画を行う場合は、対応するTyped Pipelineを明示的に使用してください。

## 実装と寿命

`TypedGraphicsPipeline<TypedVertexInput<>, ResourceSchema<>, PushSchema<...>>` を使用し、頂点3個と色を64byteの型付きpush payloadで送ります。CPU構造体とshaderのblock layoutは、既存Typedの検証を通します。shaderはSPIR-Vとしてライブラリ内に埋め込み、外部ファイルや実行時のコンパイラは不要です。

PipelineはWindowの共有データに所有させ、同じrender passでは再利用します。render passが変わった場合は次の描画時に作り直し、viewport／scissorは現在のサイズから取得します。描画ごとの頂点Buffer／descriptor／uploadはありません。GPU完了までのPipeline等の寿命は既存Typed commandが保持します。

標準の `draw(triangle, color)` はthread-localの現在のPhaseを使います。別WindowのPhaseが入れ子になっている場合、明示的なPhase指定はそのPhaseへ、`outer << pack(...)` は外側のPhaseへ記録します。呼出しはPhaseと同じスレッドで行ってください。

Phaseなし／nullptr／記録を終えたPhaseは `logic_error`、非有限の座標・色は `invalid_argument` です。失敗した入力は記録せず、正常な入力を引き続き渡せます。

## 使用例とshaderの再生成

`-DAHO_BUILD_TRIANGLE_EXAMPLE=ON` で `aho-triangle-example` をビルドできます。引数なしで対話表示、整数引数で指定フレーム数だけ描画します。

```sh
cmake --build build --target aho-triangle-example
./build/AHOLi/aho-triangle-example 120
```

元shaderは `AHOLi/AHO/src/drawer/shaders/triangle.vert`／`triangle.frag` です。変更時だけ、Vulkan SDKのglslcとLLVM20のclang-formatで埋込みヘッダーを再生成します。通常のライブラリビルドにPython／glslcは不要です。

```sh
python3 AHOLi/AHO/src/drawer/shaders/generate.py --glslc /path/to/glslc --clang-format /path/to/clang-format
```

この実装は既存Triangle APIを成立させる最初の段階です。Mesh・テクスチャ・MSDF向けの新しい描画APIは追加していません。
