# Typed拡張のstack

基点は作業開始時のmaster（`433f3e3`）から切った `typed-stack-base`。今回は以下のTyped経路を順番に実装し、各PRのbaseを直前のbranchとする。arenaによる一括確保は将来の方向として記録するが、今回のstackには追加しない。

1. descriptor: schemaをpipeline/update/bind/drawへ保持する。UBO/SSBOのpayload、画像/samplerの種別、set/binding、固定配列数を照合する。
2. push constant: payloadとstage/offset/sizeを結び、commandが値を所有する。
3. shaderのメモリ配置: UBO/SSBO/push blockのmember offset/array/matrix strideをreflectionとC++宣言で照合する。
4. indexed draw: index要素型・index bufferと頂点bindingを接続する。
5. buffer view / subpass: offset/rangeを持つviewとsubpass選択を追加する。
6. batch: pipeline/resourceの一致を確認して連続drawの冗長bindを省く。

着手時点ではTyped pipelineが頂点入力の型だけを保持し、descriptor更新は型消去されたResourceへ委譲される。descriptorの配列更新は既存APIが先頭1要素だけに固定されているため、Typed用のbackendへ配列全体の検証・更新・所有権保持を追加する。Vulkan handleは既存VSLを使う。

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
using TintRecord = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
using TintLayout = t::DescriptorLayout<t::UniformBinding<3, TintRecord, vsl::ShaderFlag::VERTEX, 2>>;
using Schema = t::ResourceSchema<t::DescriptorSetBinding<2, TintLayout>>;
t::TypedDescriptorSet<TintLayout> tints(device, manager);
tints.write<3>(std::array{first_uniform, second_uniform});
phase << pipeline.bind_vertices(vertices).bind_resources(tints).draw(3);
```

APIの型宣言と実体生成を分離しているため、将来arenaへ移行する場合もschemaの意味を変えずに確保経路を差し替えられる。

### descriptorの検証

Apple M1/macOS、LLVM 20/C++23/Debug、MoltenVK 1.4.2で関連CTest **180/180成功、skipなし**。新規はCPU 1件、実GPU 7件、compile contract 13件（正例1、負例12）。GPUはvalidation layerを有効にしてUBO/SSBO・固定配列・疎なset・同一setの別番号での再利用・texture配列・compute dispatch・初期化/usage不一致・所有権を検証した。

既存のTyped/reflection/shader/descriptor/頂点入力/bind/drawも実行。製品VSLとvsl-test、正例contract objectをビルドした。変更6ファイルを4TUで命名・配置・format検査し、違反0件。macOSのstyle frontendは製品buildと同じLLVM libc++・MacOSX26 SDKへ揃えた。Windows/Linux、sanitizer、全AHO/VSLテストは未実施。

PR: [#236](https://github.com/aruzen/AHO/pull/236)、`feat/vsl-typed-descriptors` → `typed-stack-base`。

## 2. push constantのTyped化

`PushRange<Payload, stages, offset>` と `PushSchema<Ranges...>` を追加。payloadの型・sizeof・stage・offsetを保持し、4byte単位・overflow・空stage・同stage内の重複をコンパイル時に拒否する。

`PushSchema::Values` は全rangeのpayloadを型・順序・数まで照合し、内部の固定長byte配列へコピーする。非空schemaのValuesを引数なしで作ることはできない。GPU用offsetとCPU内のpayload収納位置は別々に保持する。

`TypedGraphicsPipeline<Input, Resources, Pushes>` / `TypedComputePipeline<Resources, Pushes>` はPushesも型として保持する。必要なpushをセットするまでdraw/dispatchは制約で拒否し、binding/commandがValuesを所有する。元変数や一時Valuesの破棄後も安全に記録できる。descriptorのbindとpush値の設定はどちらの順でも行える。

```cpp
using Pushes = t::PushSchema<t::PushRange<TransformLayout, vsl::ShaderFlag::VERTEX>>;
auto bound = pipeline.bind_vertices(vertices).push_constants(Pushes::Values(transform));
phase << bound.draw(3);
```

shaderのpush memberがschemaのstageとbyte範囲で覆われていることをpipeline作成前に確認する。詳細なmember型・offset/array/matrix strideは3段目で接続する。

### 実装中に確認したVulkanの制約

最初は異なるstageなら同offsetに別値を保持できると誤解していた。実GPUのvalidationでVUID-vkCmdPushConstants-offset-01796が発生し、仕様を確認して修正した。

[vkCmdPushConstantsの仕様](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPushConstants.html)では、更新するbyteと重なるnative rangeの全stageをstageFlagsへ含める必要がある。したがって重なるbyteは共有値となる。異なるstageの重複rangeは許可するが、Values作成時にbyte列の一致を検証し、矛盾する値は例外で拒否する。異なる値が必要ならoffsetを分ける。

記録時は実際のnative range境界で更新を分割し、各区間のstageを合成する。range全体へ単純にstageを追加すると、部分重複でVUID-offset-01795に違反するため、境界ごとの分割が必要。同stageのrangeがnative backendで隙間込みに統合される場合も、実際のlayoutを参照して処理する。

### push constantの検証

同じM1/MoltenVK環境で関連CTest **206/206成功、skipなし**。新規CPU 1件、実GPU 7件、compile contract 12件（正例1、負例11）と既存のTyped/descriptor/shader/push等を確認。元変数破棄後のsnapshot、非zero offset、共有offset・異なるoffset・部分重複、descriptorとの設定順、shader側のstage/offset/extent不一致、compute dispatchを検証した。修正後のunexpected validation errorなし。

変更6ファイル/5TUの命名・配置・format検査は違反0件。詳細member配置は次段で扱うため、この段では範囲・stageとC++ payload型の契約を保証する。


PR: [#237](https://github.com/aruzen/AHO/pull/237)、`feat/vsl-typed-push-constants` → `feat/vsl-typed-descriptors`。

## 3. shader blockのメモリ配置検証

`BlockRecord<Record, Members...>` と `VSL_BLOCK_MEMBER(Record, member)` でC++のmember型・offsetofを宣言する。UBO/SSBOのbindingとPushRangeへこのlayout型を渡し、bufferやValuesは元のRecordを受け取る。未注釈の任意structをsizeofだけで通すことは廃止した。算術型・短いC配列のvector・GRAPHIC_TYPEのある既存vector型は推論できる。

```cpp
struct Tint { float rgba[4]; };
using TintRecord = t::BlockRecord<Tint, VSL_BLOCK_MEMBER(Tint, rgba)>;
using Binding = t::UniformBinding<0, TintRecord, vsl::ShaderFlag::VERTEX>;
using Pushes = t::PushSchema<t::PushRange<TintRecord, vsl::ShaderFlag::VERTEX>>;
t::TypedUniformBuffer<TintRecord> buffer(device, Tint{{1, 0, 0, 1}});
```

`std::array` は配列として扱う。短いC配列はvectorと推論するため、shaderのscalar配列を表す場合はBlockArrayで明示する。入れ子struct・配列要素の注釈にはBlockRecord/BlockArrayを使う。行列は `BlockMatrix<Scalar, columns, rows, stride, row_major>` をmemberのpolicyとして明示する。BlockValueで既存型全体にもpolicyを付けられる。数値の変換やstd140/std430の自動padding生成は行わず、実際のC++ storageを照合する。

SPIR-V reflectionにblock・memberのoffset、数値種別/bit幅、配列の各次元とstride、行列のstride/major、入れ子structを追加した。descriptor/pushのpipeline作成時に既存のreflection結果を使って照合する。宣言metadataはconstexprの参照木で、memberごとの実行時allocationは不要。reflectionの結果はコピー可能な値として保持する。

SSBO末尾のruntime配列は、C++の固定capacity配列または単独の要素型で表せる。単独要素の場合はsizeofとshaderのArrayStrideも一致が必要。固定配列はshader以上のcapacityを要求する。memberの有効extentを検証し、shader末尾のアクセスしないpaddingは強制しない。pushはstage/byte範囲の検査に加え、各shader memberを対応するC++ memberの配置と照合する。行列等の1memberを複数の別payloadへ分割する注釈は未対応で、member全体を表すpolicyを使う。

明示policyとGRAPHIC_TYPEは利用者によるstorageの意味付けであり、C++の自動reflectionではない。deviceの8/16/64bit機能有効化、画像layout、GPU同期は別の契約として残る。physical storage buffer pointerは64bitアドレスとして反映し、循環するstructを無制限に辿らない。


### block配置の検証

同じM1/MoltenVK環境で関連CTest **228/228成功、skipなし**。新規CPU 6件、実GPU 3件、compile contract 13件（正例1、負例12）。std140/std430、入れ子struct・多次元配列・行列配列・非正方行列・row/column major、8/16/32/64bit数値のreflection、runtime要素stride、同じsizeofで異なるoffset/strideを検証した。数値幅のreflectionはCPUで確認し、GPU計算は有効なdevice機能の32bit型で実行した。unexpected validation errorなし。

製品VSL・vsl-testと各正例contractをビルドし、SPIR-V validatorとMSBuild XMLの検査も成功。Windows/Linux、sanitizer、全AHO/VSLテストは未実施。

変更15ファイル/10TUの命名・配置・format検査は違反0件。

PR: [#241](https://github.com/aruzen/AHO/pull/241)、`feat/vsl-typed-block-layouts` → `feat/vsl-typed-push-constants`。

## 4. index bufferとindexed drawのTyped化

既存のBindIndexBufferはwrapperへの生pointerとuint32固定のindex型を使い、DrawIndexedはcount以外の指定ができなかった。所有権を保持するBindIndexBufferAtと16/32bitの選択を追加し、DrawIndexedへinstance count・first index・符号付きbase vertex・first instanceを追加した。既存APIはそのまま残し、新しいTyped経路では所有する方を使う。

`TypedIndexBuffer<uint16_t/uint32_t>` を `binding.draw_indexed(indices, count, instances, first_index, base_vertex, first_instance)` へ渡す。descriptor/push未設定の制約は通常drawと同じ。device、index範囲、index値+base vertexと各頂点bufferの容量、instance範囲、全native引数の整数幅を記録前に検証する。commandはpipeline・頂点/index buffer・descriptor・push snapshotを保持し、記録時にGPU完了までpinする。

index値の範囲確認用にCPU snapshotを保持し、指定範囲のmin/maxをdraw作成時に調べる。Typed経由のcopyは共有aliasのsnapshotも更新する。snapshotはGPU読み戻しではなく、GPUが生成/変更したindexやbackend()経由の直接変更を追跡しない。draw構築後の内容変更・GPU同期は利用者の責任。uint8 index、primitive restart、indirect drawは別機能として残る。現在のInputAssemblyはtriangle list / restart無効。

```cpp
t::TypedIndexBuffer<std::uint16_t> indices(device, index_values);
phase << pipeline.bind_vertices(vertices).draw_indexed(indices, index_values.size());
```

### indexed drawの検証

同じM1/MoltenVK環境で関連CTest **249/249成功、skipなし**。新規CPU 1件、実GPU 5件、compile contract 7件（正例1、負例6）。16bitのfirst index/負のbase vertex/first instance、uint16を超える32bit indexのdevice-local upload、範囲外拒否、alias更新、wrapper破棄後の所有権、descriptor/pushとの組み合わせを確認した。unexpected validation errorなし。変更8ファイル/4TUのstyle違反0件。MSBuild XML検査も成功。

PR: [#243](https://github.com/aruzen/AHO/pull/243)、`feat/vsl-typed-indexed-draw` → `feat/vsl-typed-block-layouts`。

## 5. buffer viewとsubpass選択

TypedVertexBufferの `view(first, count)` はvertex型と元bufferの所有権を保ったままbyte offset・容量を持つ。viewからのviewも相対範囲で作れ、bind/draw/indexed drawはその容量で検証する。元wrapperの寿命には依存しない。既存BufferAccessorはabstractだったため、viewはBufferImplのownerを直接保持し、BindVertexBufferAtへ渡す所有型のoverloadを追加した。viewのbackend()はこのownerを返す。

TypedUniformBuffer/TypedStorageBufferにもviewを追加し、descriptorのoffset/rangeへ接続した。payload・usageの型制約は元bufferと同じ。descriptor更新時にdeviceのminUniformBufferOffsetAlignment/minStorageBufferOffsetAlignment、範囲、最大rangeを確認する。viewの切り出し時にalignmentを丸めることはせず、不適合なら更新を拒否する。空viewは作れず、要素数による範囲超過・byte offsetのoverflowを拒否する。index bufferのviewやbyte単位の再解釈はこの段階に含めない。

`TypedGraphicsPipeline<Input, Resources, Pushes, subpass>` の第4引数でsubpassを選べる。既定値0で互換性を保ち、SUBPASS定数として参照できる。実RenderPass上の存在・attachment数/形式・sample countは既存native検査へ渡す。render pass全体やphase遷移を型に持つ機能ではなく、NextSubpassと同期は既存APIで指定する。

```cpp
auto vertices = vertex_buffer.view(first_vertex, vertex_count);
set.write<0>(uniform_buffer.view(first_uniform, 1));
using SecondSubpass = t::TypedGraphicsPipeline<Input, Resources, Pushes, 1>;
```

### view/subpassの検証

同じM1/MoltenVK環境で関連CTest **278/278成功、skipなし**。新規CPU 1件、実GPU 4件、compile contract 6件（正例1、負例5）。subviewのoffset/容量/所有権、部分頂点bufferからのdraw/indexed draw、SSBOの部分範囲だけの書き込み、uniform alignment拒否とowner保持、subpass 1の描画・存在しないsubpass拒否を確認した。unexpected validation errorなし。変更8ファイル/4TUのstyle違反0件、MSBuild XML検査成功。

PR: [#244](https://github.com/aruzen/AHO/pull/244)、`feat/vsl-typed-views-subpass` → `feat/vsl-typed-indexed-draw`。

## 6. Typed commandのbatch化

`TypedBatch(draw1, indexed2, ...)` または `TypedBatch(dispatch1, dispatch2, ...)` が完成済みのTyped commandをtupleで所有する。異なるInput/Resource/Push schemaも同じgraphics/compute系統内なら混在できる。空batch、graphicsとcomputeの混在、生command、未完成bindingはコンパイル時に拒否する。

構築時にnative実体のidentityからbind計画を作る。pipelineは実PipelineImpl、vertexはbinding番号+BufferImpl+byte offset、indexはBufferImpl+offset+要素型、descriptorはset番号+実allocationで比較する。同じC++型であっても別pipeline/buffer/setならbindする。pipeline切り替え時はdescriptorを再bindし、異なるlayoutの互換性を勝手に推測しない。vertex/index stateはVulkanの保持する実stateと照合して再利用する。push snapshotは各commandで必ず反映する。

型宣言から計算した固定容量arrayにplanと比較用stateを置くため、この計画用にheap allocationは追加しない。command内の既存resource vectorやnative objectのshared ownershipは継続する。`statistics()` で計画上のpipeline/vertex/index/descriptor bind数とdraw/dispatch数を確認できる。

batchの各invokeは最初から必要なstateをbindする。外側のcommand streamや前回のbatch実行のstateには依存しない。get_pipelineは最後のcommandのpipelineを返し、直後の動的APIへ正しい状態を伝える。元wrapper/batchの破棄後もsubmissionへownerをpinする。全commandは同じdeviceを要求し、異なるmanagerは記録前に拒否する。

順序変更・drawの統合・GPU barrier挿入は行わない。compute dispatch間のデータ依存、descriptorの使用中更新、pipelineに対応するrender pass/subpass、CPU/GPU同期は利用側が扱う。依存するdispatchの間にbarrierが必要ならbatchを分け、既存commandでbarrierを入れる。今回のcompute GPU検証は別々の出力範囲へ書き込む2dispatchを使う。

```cpp
auto bound = pipeline.bind_vertices(vertices).bind_resources(resources);
auto batch = t::TypedBatch(bound.draw(3), bound.draw(6, 1, 3));
phase << batch;
```


### batchの検証

同じM1/MoltenVK環境で関連CTest **290/290成功、skipなし**。新規実GPU 6件、compile contract 6件（正例1、負例5）。同じpipelineのbind省略とbatch再利用、同じ型の別pipelineと最後のpipelineの通知、vertex bufferのidentity/offset、index型、descriptorの切り替え・pipeline変更時の再bind、全push snapshot、wrapper/batch破棄後の所有権、computeの別範囲への出力を確認した。unexpected validation errorなし。変更5ファイル/2TUのstyle違反0件。新規SPIR-V validator、MSBuild XML、diff検査も成功。

6段の機能はそれぞれのbaseから製品VSL・vsl-test・正例contractをビルドし、各段の新規テストと既存経路を実行した。最終290件はTypedの全compile contract 73件と関連CPU/GPUテストを含む。全AHO/VSLテスト、Windows/Linux、sanitizerは未実施。masterへのmergeは行わず、最後のbranchは `feat/vsl-typed-batches`、baseは `feat/vsl-typed-views-subpass`。
