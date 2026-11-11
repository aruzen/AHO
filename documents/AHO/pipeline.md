# Pipelineの責務

AHOは用途別のlayout既定値を提供し、pipeline本体と型検査はVSLを使用します。`AHO/pipeline/pipeline.hpp`は既定値とVSLの型付きpipelineヘッダーをまとめて読む入口です。AHO独自のpipelineラッパーは作りません。

| 用途 | 使用する型・機能 |
| --- | --- |
| 型付きのgraphics | `vsl::typed::TypedGraphicsPipeline<Input, Resources, Pushes>` |
| 型付きのcompute | `vsl::typed::TypedComputePipeline<Resources, Pushes>` |
| 専門的な制御 | `vsl::GraphicsPipeline`・`vsl::ComputePipeline` |
| AHOのlayout既定値 | `aho::pipeline::GET_BASE_LAYOUT`・`get_binding_point` |

描画先やデバイス、descriptor manager等はWindowの`RenderContext`から取得できます。[Typedサンプル](../../AHOLi/AHO/examples/typed/scene.cpp)が、入力・資源・push定数のschemaとpipeline作成、bind、描画を組み合わせる実例です。AHOのBufferを使う場合は[Typedとの連携](../typed-buffer-interop.md)を参照してください。

実装のなかった`aho::PipelineData`・`aho::ComputePipeline`・`aho::GraphicPipeline`と、未実装factory向けの`StandardPipelineTag`・`DefinedDescriptorBufferTag`は削除しました。型を消したフィールドの詰め合わせやタグを渡す形から、VSLのpipelineと入力・resource・pushのschemaを明示する形へ移行します。`AHO/pipeline/layouts.hpp`の既存presetは維持します。
