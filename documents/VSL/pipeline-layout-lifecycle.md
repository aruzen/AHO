# PipelineLayoutの編集と凍結

`PipelineLayout`は編集用、`FrozenPipelineLayout`は構築済みの型です。

```cpp
vsl::PipelineLayout editable(device, shader_group, vertex_input, color_blend,
                             input_assembly, rasterization, multisample,
                             dynamic_state, viewport, scissor);
editable.add(additional_stage);
auto frozen = std::move(editable).freeze();
vsl::GraphicsPipeline pipeline(frozen, render_pass);
```

`freeze()`は右辺値だけに使用できます。元の変数は消費され、再利用するには新しい`PipelineLayout`を代入してください。凍結済みの値に`add()`、`copy()`、ステージ注入は提供しません。Pipeline生成とPushConstantなどの参照には共通の`PipelineLayoutAccessor`を使えます。凍結済みの値自体はコピーできます。

従来の`layout.freeze()`は`std::move(layout).freeze()`へ移行してください。独自ステージの`injection`は`PipelineLayoutAccessor`から`PipelineLayoutEditAccessor`へ変更します。内部実装ポインターから編集用オブジェクトを作る公開コンストラクターは廃止しました。

`add()`は編集対象だけを置き換えます。コピー代入した別名や既存Pipelineが保持する状態は変化しません。ステージ注入・native生成に失敗した場合も元の状態を維持します。`copy()`は配列とnative layoutを独立させます。Shader、BindingLayoutはruntimeとは別所有のrecipeが保持します。

凍結後もGraphics／ComputePipelineを生成できるよう、Frozenはrecipeを保持します。作成済みPipelineはruntimeだけを保持し、生成用Shaderまで保持し続けません。freezeはnativeを再生成せず、編集可能なrecipeの別名がある場合だけrecipeをコピーします。追加のGPU送信・完了待ちは発生しません。

標準配列はrecipeの型付きvectorに所有し、native pointerは生成scratchへ組み立てます。privateのCreateInfo／map<string, any>は廃止しました。独自の内部stageはedit_recipe()へ移行します。recipe()／edit_recipe()は一時オブジェクトから呼べません。data_は後続段階で廃止する内部実装用入口です。同じ編集用オブジェクトへの並行操作には呼び出し側の同期が必要です。配置・所有と移行の詳細は[PipelineLayout arena](pipeline-layout-arena.md)を参照してください。
