# Typed入力から描画までの静的な契約

目的は、頂点layoutの情報をコンパイル時に解決し、pipeline・buffer・bind/drawまで型として保持すること。PR #151の既存VertexInputへの接続を取り込み、最新masterの命名へ合わせて拡張する。

今回の範囲は頂点入力の一連の経路。descriptor、push constant、runtime SPIR-Vとの整合性、indexed drawは残る拡張点として記録する。

- `TypedVertexInput` がbinding/location/offset/strideをconstexpr配列へ解決し、重複・overflow・不正なextentをコンパイルで拒否する。
- `VertexBinding<Vertex,...>` がC++レコード型と属性宣言を結び、`TypedVertexBuffer<Binding>` がその型のデータを保持する。
- `TypedGraphicsPipeline<Input>` とbindの戻り値がInputを保持し、異なるbinding型・不足/余分なbufferをコンパイルで拒否する。
- draw commandはpipelineとbufferを所有し、記録時にpipelineと全bindingを明示する。wrapper破棄後もGPU完了まで基盤VSLのretainを使用する。
- データ件数・instance数・device一致・GPUの物理的制限は実行時の情報。Vulkanのhandle生成も実行時のまま。

64bitの3/4成分入力はlocationを二つ使用するため、自動locationと重複検査にも反映する。[Vulkan仕様](https://docs.vulkan.org/spec/latest/chapters/fxvertex.html)

## 利用方法

`<VSL/utils/typed_pipeline.hpp>` をincludeする。`typed_safe_resource.hpp` から既存の型付き頂点宣言をincludeする経路も維持する。以下は、device・render pass・shader/fixed-function stageを既に用意した場合の例。

```cpp
namespace t = vsl::typed;
struct Position { float xy[2]; };
struct Color { float rgb[3]; };
using Positions = t::VertexBinding<Position, 0, t::VertexTiming::NEXT_VERTEX,
    t::VertexInputDefinition<vsl::data_format::FLOAT_RG, 0, offsetof(Position, xy)>>;
using Colors = t::VertexBinding<Color, 1, t::VertexTiming::NEXT_VERTEX,
    t::VertexInputDefinition<vsl::data_format::FLOAT_RGB, 1, offsetof(Color, rgb)>>;
using Input = t::TypedVertexInput<Positions, Colors>;

static_assert(Input::LAYOUT.binding[0].stride == sizeof(Position));
t::TypedGraphicsPipeline<Input> pipeline(device, pass, shader_group,
    color_blend, input_assembly, rasterization, multisample, dynamic_state, viewport, scissor);
t::TypedVertexBuffer<Positions> positions(device, position_records);
t::TypedVertexBuffer<Colors> colors(device, color_records);
auto bindings = pipeline.bind_vertices(positions, colors);
phase << bindings.draw(3);
```

`bind_vertices(colors, positions)`、buffer不足・余分なbuffer、異なるbinding宣言のbufferはコンパイルできない。`TypedVertexBuffer<Positions>` に同じバイト数の別レコード型を渡すことも拒否する。

構造体レコードの属性offsetは `offsetof` 等で明示する。C++には構造体の全memberを自動列挙してoffsetを知る仕組みを要求していないため、packed配置を勝手に構造体へ適用しない。型を伴わない既存の `VertexShapeDefinition` ではpackedな自動offsetを引き続き扱う。recordのformat宣言が実際のmemberの解釈に合うことは利用側が指定し、shaderのSPIR-Vとの一致は別の検証層で確認する。

instance入力は `NEXT_INSTANCE` を指定し、`draw(vertex_count, instance_count, first_vertex, first_instance)` を使う。binding番号は疎でもよく、bufferの引数順はInputの型宣言順とする。内部の `BindVertexBufferAt` が実際のbinding番号へ接続する。

## 実行時の契約

- pipelineの生成とGPU handleは既存VSLへ委譲する。Typed pipelineはInputを最後に注入し、他のstageが型付き頂点layoutを上書きしないようにする。
- `bind_vertices` はGPU記録を行わず、pipelineとbufferを保持する値を返す。`draw` が件数を検査してcommandを作り、`phase << command` でpipeline・全binding・drawをまとめて記録する。
- commandはPipelineHolderとして既存のstreamへpipelineを伝える。記録時には各resourceをCommandBufferへretainし、commandやC++ wrapperが破棄されてもGPU完了まで生存させる。
- `copy` は同じrecord数の更新に限定する。GPU実行中のbufferへCPUから上書きしない同期は利用側で行う。DeviceLocal bufferはmanager付きコンストラクタで初期uploadできる。
- `backend()` による既存APIへの接続は明示的に行う。この経路でlayoutやbufferの実体を置き換える場合、Typed APIの保証を利用側で維持する。
- コンパイル時の照合は宣言layoutとrecord/binding型の関係に対するもの。実行時ロードしたSPIR-Vの入力との一致や、device固有のformat対応・最大数等はVulkan/reflectionによる実行時確認が必要。

## 残る抽象化・拡張

| 項目 | 現状と次の接続 |
|---|---|
| descriptorとpush constant | 既存の型付き宣言はあるが、今回のTyped pipelineは頂点入力だけを型として保持する。次にpipelineの型へresource schemaを持たせ、UBO/SSBO/textureとpush payloadの型照合へ広げる。 |
| shader reflection | runtime reflectionのUBO/SSBO member offset/stride情報とCPU layoutの検証、またはビルド時SPIR-Vからの型生成が必要。 |
| indexed draw | indexの要素型・index範囲とvertex bindingを結ぶTyped経路が必要。現状はnon-indexed drawのみ。 |
| buffer view / subpass | 現状はbuffer先頭をbindingし、pipelineはsubpass 0を利用する。typed viewのoffset/rangeとsubpass指定を拡張する。 |
| bindの省略・batch | 現状は各TypedDrawがpipelineと全bufferを明示する。複数drawを型付きbatchとしてまとめ、整合性を保ったまま冗長bindを省く余地がある。 |

最初のビルド時には、既存Format型にdefault constructorがないため、定数評価用の属性配列へUNDEFINEDの明示初期値が必要だった。定数評価の失敗を修正し、全体をconstexprで生成できるようにした。

## 検証

2026-10-22、Apple M1/macOS、Homebrew LLVM 20、C++23/Debug、Vulkan SDK 1.4.309、MoltenVK 1.4.2で確認。関連CTest **64/64成功、skipなし、validation errorなし**。

- compile contract 11件: 正例1件と誤ったbinding/record、buffer不足、重複binding/location、extent overflow、短いstride、構造体の暗黙offset、64bit属性のlocation重複、未定義formatを拒否する10件。失敗が想定した診断によることも確認する。
- 新規実GPU 9件: 通常・疎なbinding、instance入力とfirst vertex/instance、入力なしのprocedural vertex、範囲外draw拒否、wrapper/command破棄後のretain、DeviceLocal upload、floatと正規化整数の混在/padding、既存動的APIへの明示接続をreadbackで確認。
- 新規の定数評価2件に加え、既存のTyped resource/VertexInput/stride/buffer bind/drawの関連テストを確認。従来disabledだったTypedVertexInputの注入テストも有効化した。
- VSLの全製品ソースとvsl-testをこのworktreeのソースでビルド。ヘッダーの差し替えや別ABIのarchiveは使用しない。MSBuild XMLとdiff checkも確認。

再実行は `cmake --build build/typed --target vsl-test` 後、`ctest --test-dir build/typed --output-on-failure -R 'Typed|VertexInput|VertexStride|BindVertexBuffer|DrawTest|DrawCount|DrawVulkan'`。Vulkan loaderが利用するICDとvalidation layerの場所は実行環境に合わせて指定する。

全VSL/AHOテスト、Windows/Linux、sanitizer、異なるdevice同士を組み合わせた実GPU検証は今回の範囲外。
