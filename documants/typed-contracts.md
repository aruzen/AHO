# Typed APIの所有・宣言・shader契約の整理

## 画像の読み取りステージ（PR #251）

`ResourceBindingPoint`の`MakeShaderReadBarrier(image)`で、sampled imageのbinding宣言から転送後の読み取り同期を作れる。shader_typeを既存のChangeImageBarrierへ伝え、画像がSAMPLED用途を持つこと、bindingがSAMPLED_IMAGE/COMBINED_IMAGE_SAMPLERであること、shader stageが有効な非空集合であることをコンパイル時に検査する。型消去されたImageAccessorを直接渡す経路は持たない。

Typedの頂点入力・vertex buffer・drawの型や処理は変更しない。コマンドの生成時にpipeline stageへ変換し、draw時のreflectionや自動待機は増やさない。使用例と制約は[画像同期の説明](../documents/VSL/image-shader-visibility.md)を参照。

次のTyped整備では、`TypedGraphicsPipeline<Input>`へresource schemaを結び、descriptorの番号・種別・配列数・shader stageをロード済みSPIR-Vと照合し、型付きresource bindingからdrawまで保持する経路を扱う。画像layoutの実行履歴と状態型を結ぶ設計は別途必要で、今回のfactoryだけでlayout遷移を保証するものではない。

## これまでの整備

PR #221の後続。次の3点を順に解決する。

1. `TypedVertexBuffer<Binding>` がbinding番号・location・入力頻度まで所有型へ結び付けており、同じGPUデータを別pipelineへ再利用しにくい。bufferをレコード型へ結び、接続情報はpipelineのbindingへ分離する。
2. C++レコードとformat/offsetの対応をbindingごとに繰り返す。レコードlayoutを一度定義し、binding宣言と検査用metadataをそこから導出する。C++のpaddingを尊重し、正規化整数のようなformatの意図も明示できるようにする。
3. Typedの静的検査だけでは、ロードしたSPIR-Vとの一致を確認できない。実際にpipelineへ注入されたvertex shaderをreflectionで照合し、不一致をpipeline作成前に診断する。C++側のコンパイル時保証、shader側の実行時保証、device検証の境界を記す。

向け先は統合済み #221 の `typed-pipeline-base`。実装・検証結果は各段階で追記する。

## 1. bufferとbindingの分離

`TypedVertexBuffer<Vertex>` はレコード型だけを保持する。`VertexBinding<Vertex,...>` と `TypedVertexBinding<Input>` がpipelineへの接続を受け持つ。同じレコードのbufferはbinding番号・location・入力頻度の違うpipelineへそのまま接続でき、再uploadやbackendへの型消去は不要。bufferの引数順・数・レコード型は引き続きコンパイル時に検査する。

既存Typedテストとcompile contractを新しいAPIへ移行。同じallocationを通常bindingと疎なbindingのpipelineへ渡し、両方のGPU readbackを確認する。

## 2. レコードlayoutを一度定義する

`VertexRecord<Vertex, Members...>` を導入。`VSL_VERTEX_MEMBER(Vertex, member, location?, format?)` はメンバー型とoffsetofを一緒に渡し、通常のscalar・C配列・std::arrayのformatを推論する。既存の `GRAPHIC_TYPE` を持つ型にも接続する。正規化整数等はformatを明示できる。

`Record::Binding<binding, timing, first_location>` は共通layoutからbindingを生成する。locationはレコード内の相対値を一度解決し、first_locationを加算する。strideはsizeof(Vertex)、offsetはoffsetofを使う。異なるrecordのmember、memberのstorageを超えるformat、未対応型、location overflowはコンパイル時に拒否する。明示formatの意味上の解釈は呼び出し側が選ぶ。

C++23ではmember pointerからportableにconstexpr offsetofを求められないため、マクロは型とoffsetの取得だけに限定した。layout生成と検査はテンプレートで行う。padding付き構造体、float/正規化整数の混在、std::array<double,3>の2location消費を検証する。

自動推論の配列はscalarの1〜4要素をvectorとして扱う。配列の配列を一つのvectorへ誤変換しないよう、行列やvector配列の自動推論は拒否する。これらは低水準の属性宣言で列/要素を指定する。`std::int8_t` のsigned charにも対応し、std::arrayや `GRAPHIC_TYPE` を持つ独自型も検証する。

```cpp
namespace t = vsl::typed;
struct Vertex { float position[2]; std::uint8_t color[4]; };
using Record = t::VertexRecord<Vertex,
    VSL_VERTEX_MEMBER(Vertex, position),
    VSL_VERTEX_MEMBER(Vertex, color, t::AUTO, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA)>;
using Input = t::TypedVertexInput<Record::Binding<0>>;
using OtherInput = t::TypedVertexInput<Record::Binding<4>>;

t::TypedVertexBuffer<Vertex> buffer(device, records);
// Input/OtherInputのpipelineで同じbufferを共有する。
phase << pipeline.bind_vertices(buffer).draw(3);
phase << other_pipeline.bind_vertices(buffer).draw(3);
```

## 3. 実際のSPIR-Vとの照合

Typed pipelineは、注入後のlayoutに含まれるvertex shaderを調べ、ロード済みのSPIR-Vと実際のentry pointからactiveな頂点入力を取得する。元のファイルの削除・書き換えに検査結果が左右されないよう、ShaderImplがロードしたword列を保持する。検査はgraphics pipeline作成前の一回で、draw時のreflectionは行わない。

location、Component、float/signed/unsignedの数値分類、64bit幅を照合する。配列と行列は要素/列へ展開する。UNORM/SNORM/SCALED等からfloatへの変換、および16/32bit入力の不足成分への既定値は許可する。64bitには既定値がないため、不足成分も拒否する。使わない余分な属性は許可する。

不一致はentry point・入力名・location・componentと理由を含む例外で通知する。vertex stage不足/重複も拒否する。GPUがなくても `validate_vertex_shader<Input>(reflector.generated)` を呼んで同じ照合を使える。

Typed pipelineでは `SPIRVReflector::Scope::VERTEX_INPUT` を使用し、descriptor/push constantのreflectionを省く。頂点入力だけの検査がdescriptorのruntime array等を制限しないようにした。SPIRV-CrossのparseエラーはVSLのRuntimeExceptionへ理由を保持して変換する。

仕様根拠: [Vulkan: Fixed-Function Vertex Processing](https://docs.vulkan.org/spec/latest/chapters/fxvertex.html)。formatの完全一致ではなく、Vulkanの数値変換とlocation/component規則に沿って検査する。

### 保証の境界

| 段階 | 保証する内容 |
|---|---|
| C++コンパイル時 | レコード型・buffer数/順序・layoutの重複/overflow・memberの範囲・stride・推論format |
| pipeline作成時 | 実際のvertex entry pointと上記layoutの入力互換性 |
| draw作成時 | buffer件数に対するvertex/instanceの範囲、uint32範囲 |
| GPU/Vulkan | device固有のformat/feature/limit、descriptor、同期等 |

descriptor/UBO/SSBO/push constantのmember layoutとpayload型、シェーダー内部の計算、意図したmemberの意味、明示backend操作後の不変条件はこの契約に含まない。現状のTyped pipelineが保持するschemaは頂点入力用。これらを「静的に保証済み」とは扱わない。

## 検証結果

2026-10-23、Apple M1/macOS、LLVM 20/C++23/Debug、Vulkan SDK 1.4.309、MoltenVK 1.4.2で関連CTest **121/121成功、skipなし**。TypedのGPUテストはvalidation layerを有効にし、unexpected validation errorなしで完了した。

- 最初の分離後は関連Typed 34件、レコード宣言追加後は39件を確認。
- 最終状態のcompile contractは16件（正例1、想定した診断で拒否される負例15）。誤ったレコード/メンバー、buffer数、重複、overflow、未定義format、暗黙offset、未対応の行列推論等を検証。
- Typed layoutのCPUテスト4件、shader interfaceのCPUテスト11件。shader側の配列/行列、Component、64bitの2location、数値型、正規化/既定値、active input、entry point選択、runtime descriptor arrayの分離、entry pointのない不正moduleを確認。
- Typed pipelineの実GPUテスト13件。同一allocationの別bindingでの共有、既存のinstance/first index・寿命・DeviceLocal upload・混在formatに加え、shader不一致の事前拒否、ロード元ファイル削除後の描画、vertex stage不足/重複、複数entry pointの実描画を確認。
- 既存のTyped resource、reflection、shader、VertexInput、stride、buffer bind、drawの関連テストも実行。
- 全VSL製品ソースとvsl-testをこのworktreeでビルド。新規header/sourceをCMake/MSBuildへ登録し、MSBuild XML・git diff checkを確認。新規の通常/数値/64bit/複数entry pointのfixtureはspirv-valでも検証した。

再実行:

```sh
cmake --build build/typed --target vsl-test -j6
ctest --test-dir build/typed --output-on-failure -j3 \
  -R 'Typed|SpirvReflector|ShaderTest|ShaderVulkan|VertexInput|VertexStride|BindVertexBuffer|DrawTest|DrawCount|DrawVulkan'
```

VulkanのICD/validation layerの場所は環境に合わせて指定する。Windows/Linux、sanitizer、全AHO/VSLテストは未実施。従来の動的APIは維持し、Typed bufferの利用側はテンプレート引数をBindingからVertexへ移行する。
