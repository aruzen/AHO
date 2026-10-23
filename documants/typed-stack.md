# Typed拡張のstack

基点は最新masterから切った `typed-stack-base`。今回は以下のTyped経路を順番に実装し、各PRのbaseを直前のbranchとする。arenaによる一括確保は将来の方向として記録するが、今回のstackには追加しない。

1. descriptor: schemaをpipeline/update/bind/drawへ保持する。UBO/SSBOのpayload、画像/samplerの種別、set/binding、固定配列数を照合する。
2. push constant: payloadとstage/offset/sizeを結び、commandが値を所有する。
3. shaderのメモリ配置: UBO/SSBO/push blockのmember offset/array/matrix strideをreflectionとC++宣言で照合する。
4. indexed draw: index要素型・index bufferと頂点bindingを接続する。
5. buffer view / subpass: offset/rangeを持つviewとsubpass選択を追加する。
6. batch: pipeline/resourceの一致を確認して連続drawの冗長bindを省く。

現在はTyped pipelineが頂点入力の型だけを保持し、descriptor更新は型消去されたResourceへ委譲される。descriptorの配列更新は既存APIが先頭1要素だけに固定されているため、Typed用のbackendへ配列全体の検証・更新・所有権保持を追加する。Vulkan handleは既存VSLを使う。

同期とdescriptorの使用中更新は利用側の責任。型検査はGPU実行中の更新を安全にする仕組みではない。特殊descriptor（texel buffer、dynamic offset、acceleration structure等）は対応するnative機能と合わせた拡張が必要で、今回の共通UBO/SSBO・画像・samplerの契約には含めない。

## 将来のメモリ計画（今回の実装対象外）

- 宣言schemaを唯一の入力とし、型の整合性検査、native layout生成、必要オブジェクト数、CPU storageのsize/alignmentを導出する。
- 動的な容量（頂点数、フレーム数等）は型または明示的なcapacity引数へ集約する。payloadの論理サイズとnative object metadataのサイズを区別する。
- arenaの所有者は領域全体を所有する。内部の依存参照は借用handleとし、個々のオブジェクトから同じarenaを所有して循環参照を作らない。
- 外部wrapper/commandは必要な範囲でarenaをpinする。GPU完了まで領域を解放せず、最後のpinが外れた後に逆構築順で破棄する。
- 構築失敗時の巻き戻し、alignment、overflow、空のplan、未構築slot、二重構築、領域の寿命を検証する。placement newだけに置換して所有権を曖昧にしない。
- CPU側のobject layoutとGPU allocation planは分ける。GPUのrequirements・alignment・memory type・専用allocation要否はdevice/driverから取得して実行時に確定する。論理payload合計をVulkanの実allocation sizeとは扱わない。
- 既存の動的APIとの接続は保つが、Typed経路では確保方針を外から渡せるようにし、段階的に既存shared_ptrによる個別確保を置換する。

今回はschemaと実体生成を分離し、既存VSLの所有方式で各Typed機能を実装する。個別の所有・確保をarenaへ変更する作業は後続の別課題とする。

## 1. descriptorのTyped化

- `DescriptorLayout<Bindings...>` がset内のlayout、`DescriptorSetBinding<set, Layout>` がpipeline上の配置を表す。`DescriptorSet<set, Bindings...>` はこの2つを組み合わせる宣言用の別名。実際のdescriptor setはlayoutの型だけで再利用でき、別set番号のpipelineへ同じallocationを接続できる。
- `ResourceSchema<Sets...>` はconstexprでset/binding・種別・stage・固定配列数を保持する。pipelineへschemaを最後に注入し、宣言と実layoutが食い違わないようにする。疎なset番号の欠番には空layoutを作る。
- `TypedUniformBuffer<T>` / `TypedStorageBuffer<T>` はpayload型とusageを保持する。`TypedDescriptorSet<Layout>::write<binding>` はpayload・種別・配列数を制約し、全要素を更新してbuffer/image/samplerの所有権を保持する。画像のdevice/usageは実行時に検査する。
- `TypedGraphicsPipeline<Input, Schema>` はvertex binding後に `bind_resources(...)` を呼ぶまでdrawできない。`TypedComputePipeline<Schema>` も必要なsetを揃えてからdispatchする。初期化不足はbind時に拒否し、commandはresourceをGPU完了まで保持する。
- 実際のshader entry pointとset/binding・種別・配列数・stageをpipeline作成前に照合する。member offset/stride等のpayload配置検証は3段目で追加する。
- Descriptor自身のruntime配列、dynamic offset、texel buffer、acceleration structure等はこの段階の対象外。SSBO内部のruntime配列にはTypedStorageBuffer<T>の要素数を使える。画像layout遷移、CPU/GPU同期、使用中descriptor更新の制御は既存VSLと同じく利用側の責任。

```cpp
using TintLayout = t::DescriptorLayout<t::UniformBinding<3, Tint, vsl::ShaderFlag::VERTEX, 2>>;
using Schema = t::ResourceSchema<t::DescriptorSetBinding<2, TintLayout>>;
t::TypedDescriptorSet<TintLayout> tints(device, manager);
tints.write<3>(std::array{first_uniform, second_uniform});
phase << pipeline.bind_vertices(vertices).bind_resources(tints).draw(3);
```

APIの型宣言と実体生成を分離しているため、将来arenaへ移行する場合もschemaの意味を変えずに確保経路を差し替えられる。

### descriptorの検証

Apple M1/macOS、LLVM 20/C++23/Debug、MoltenVK 1.4.2で関連CTest **180/180成功、skipなし**。新規はCPU 1件、実GPU 7件、compile contract 13件（正例1、負例12）。GPUはvalidation layerを有効にしてUBO/SSBO・固定配列・疎なset・同一setの別番号での再利用・texture配列・compute dispatch・初期化/usage不一致・所有権を検証した。

既存のTyped/reflection/shader/descriptor/頂点入力/bind/drawも実行。製品VSLとvsl-test、正例contract objectをビルドした。変更6ファイルを4TUで命名・配置・format検査し、違反0件。macOSのstyle frontendは製品buildと同じLLVM libc++・MacOSX26 SDKへ揃えた。Windows/Linux、sanitizer、全AHO/VSLテストは未実施。
