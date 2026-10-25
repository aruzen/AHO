# 画像アップロード後のシェーダー可視性

`ChangeImageBarrier(image, TRANSFER_DST_OPTIMAL, SHADER_READ_ONLY_OPTIMAL)`は、転送書き込みと後続のシェーダー読み取りを同期する。Computeや頂点シェーダーでも画像をサンプリングできるため、フラグメントシェーダーだけに可視性を限定しない。

送信元はTransfer/TransferWrite、送信先のaccessはShaderReadを使う。ステージを指定しない既存の呼び出しはAllCommandsを使い、別ステージからの読み取りにも対応する。GPU上の実行依存はFragmentShader固定の実装より広がるため、用途が分かる場合はShaderFlagで必要なステージを指定する。

```cpp
phase << command::ChangeImageBarrier(image, ImageLayout::TRANSFER_DST_OPTIMAL,
                                     ImageLayout::SHADER_READ_ONLY_OPTIMAL, ShaderFlag::FRAGMENT);
phase << command::ChangeImageBarrier(other_image, ImageLayout::TRANSFER_DST_OPTIMAL,
                                     ImageLayout::SHADER_READ_ONLY_OPTIMAL, ShaderFlag::COMPUTE);
```

複数ステージは`ShaderFlag::VERTEX | ShaderFlag::COMPUTE`のように指定する。配列画像は従来のbase_layer/layer_countの後にShaderFlagを渡せる。明示指定はTRANSFER_DST_OPTIMALからSHADER_READ_ONLY_OPTIMALへの遷移で使用し、空のステージ・未知のビット・他のlayout遷移との組み合わせを生成時に拒否する。生成後に公開layoutフィールドを変更して指定を無効化した場合も記録前に拒否する。

ShaderFlagとVulkanのpipeline stageはビット位置が異なるため個別に変換する。変換結果はコマンド生成時に保持し、記録時に再計算しない。ALL_GRAPHICSはVulkanのAllGraphicsへ変換する。個別のGeometry/Tessellation/Task/Mesh/RayTracingを指定する場合は、利用側で必要なdevice featureやextension、queueの対応を満たす。明示指定による新しいGPU送信・queue/device待機・実行時reflection・メモリー確保は行わない。

AHOの通常サンプルはtexture bindingのshader_typeをそのまま渡し、FragmentShaderへの狭い同期を使用する。AllCommandsと狭い同期の速度差はGPUや処理の並びに依存し、性能ベンチマークは未実施。

## Typedのbinding宣言から作る

```cpp
using Texture = typed::ResourceBindingPoint<2, graphic_resource::Type::COMBINED_IMAGE_SAMPLER,
                                           ShaderFlag::COMPUTE>;
// imageはImageType::SAMPLEDを含むImageで、TRANSFER_DST_OPTIMALへの遷移と転送は済んでいる。
phase << Texture::MakeShaderReadBarrier(image);
```

bindingのshader_typeを同期先へ渡すので、descriptor宣言とバリアでステージを二重に書かずに済む。SAMPLED_IMAGE/COMBINED_IMAGE_SAMPLER以外のbinding、SAMPLED用途のない画像、型消去されたImageAccessor、空・不明なshader stageはコンパイル時に拒否する。base_layer/layer_countも指定できる。

この入口はTypedの頂点入力やdraw APIを変更しない。TypedGraphicsPipelineはまだresource schemaを型として保持していないため、drawから自動的に必要な画像バリアを挿入する機能ではない。複数binding/pipelineから同じ画像を読む場合、次の転送までに読み取る全ステージを指定する必要がある。実際の画像layout履歴の追跡も利用側の責任となる。

既存のCompute画像テストにあった追加のnativeバリアを削除し、公開コマンドだけで同期する。未指定、Compute指定、Typed宣言、複数ステージ、AllGraphics+Computeの各経路で、転送した画素がComputeのサンプリング結果へ反映されることと同期エラーがないことを確認する。対象ケースだけSynchronization Validationとshader access追跡を有効にし、終了時に環境を復元する。

参照: [Khronosのpipeline barrier性能解説](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html)、[pipeline stageの定義](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineStageFlagBits.html)。
