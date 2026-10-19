# PipelineLayoutの編集と凍結

`PipelineLayout`は編集用、`FrozenPipelineLayout`は構築済みの型です。

```cpp
vsl::PipelineLayout editable(device, shaderGroup, vertexInput, colorBlend,
                             inputAssembly, rasterization, multisample,
                             dynamicState, viewport, scissor);
editable.add(additionalStage);
auto frozen = std::move(editable).freeze();
vsl::GraphicsPipeline pipeline(frozen, renderPass);
```

`freeze()`は右辺値だけに使用できます。元の変数は消費され、再利用するには新しい`PipelineLayout`を代入してください。凍結済みの値に`add()`、`copy()`、ステージ注入は提供しません。Pipeline生成とPushConstantなどの参照には共通の`PipelineLayoutAccessor`を使えます。凍結済みの値自体はコピーできます。

従来の`layout.freeze()`は`std::move(layout).freeze()`へ移行してください。独自ステージの`injection`は`PipelineLayoutAccessor`から`PipelineLayoutEditAccessor`へ変更します。内部実装ポインターから編集用オブジェクトを作る公開コンストラクターは廃止しました。

`add()`は編集対象だけを置き換えます。コピー代入した別名や既存Pipelineが保持する状態は変化しません。ステージ注入・native生成に失敗した場合も元の状態を維持します。`copy()`は配列とnative layoutを独立させます。Shader、BindingLayoutは生成情報が所有します。

従来のfreezeによる生成情報の破棄は行いません。凍結後もGraphics/ComputePipelineを生成できるよう、生成情報とその所有者を保持します。追加のGPU送信・完了待ちは発生せず、編集・独立コピー時だけnative layoutを生成します。共有していない値のfreezeはハンドルを再生成しません。

独自ステージが直接native構造体のポインターを設定する場合、その参照先はステージ側で寿命とコピーを管理してください。VSL標準ステージの配列はコピー時に参照先を張り直します。`_data`は内部実装用であり、直接書き換えによる凍結解除はAPIの対象外です。同じ編集用オブジェクトへの並行操作には呼び出し側の同期が必要です。
