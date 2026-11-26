# Typed bundleのImage／Sampler配置

[完了計画](arena-completion-plan.md)の順序3。Bufferに加え、schemaが所有するImage／Samplerを[ResourceArena](resource-arena.md)へ配置する。CPUのImpl＋UseStateの領域をまとめる変更であり、GPU device memoryは各資源の共通backendで確保する。

## 宣言と使用

```cpp
namespace t = vsl::typed;
using Resources = t::ResourceSchema<t::DescriptorSet<0,
    t::TextureBinding<0, vsl::ShaderFlag::COMPUTE>,
    t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>>;
using Schema = t::ResourceAllocationSchema<Resources,
    t::TextureAllocation<0, 0>, // frame Image＋shared Sampler
    t::BufferAllocation<0, 1, t::FixedElements<2>>>;
t::ResourceAllocationSizes<Schema> sizes;
sizes.image<0, 0>({vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST,
                  64, 64, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA});
sizes.sampler<0, 0>({vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE});
t::TypedResourceBundle<Schema, t::ArenaStorage<>> bundle(manager, descriptors, sizes);
bundle.update(phase, [&](auto& frame) {
    phase << vsl::command::ImageUpload(&pixels, frame.template image<0, 0>(),
                                      vsl::ImageLayout::UNDEFINED);
});
phase << bundle.get(phase).bind_resources(pipeline).dispatch(1);
```

`pixels`には画像に合う転送データ、`pipeline`にはResourcesに合うshaderを渡す。初回はUNDEFINED、再更新では実際の現在layoutを渡す。画像のcontent／layoutは自動推論しない。Bufferは従来どおりゼロ初期化するが、Imageは明示的に初期化する。

`ImageAllocation<set,binding,scope>`はSAMPLED_IMAGE／STORAGE_IMAGE、`SamplerAllocation`はSAMPLER、`TextureAllocation<set,binding,image_scope,sampler_scope>`はCOMBINED_IMAGE_SAMPLERへ対応する。Imageの既定scopeはPER_FRAME、SamplerはSHARED。Textureのdescriptor／facadeのscopeは、どちらかのcomponentがPER_FRAMEならPER_FRAMEになる。

画像のextentは必須。`image()`／`sampler()`へdescriptor配列数と同じ`std::array`を渡せば、要素ごとに異なる作成infoを指定できる。layersやpayload要素数はImpl個数を表さない。Sampler未指定時は通常のSamplerと同じLINEAR／CLAMP_TO_BORDER。Buffer用`set(elements)`の意味は維持する。

通常のheap配置は次の指定で利用する。Bufferは既存TypedResourceBuffer、Image／Samplerは通常VSLの生成処理を使う。arenaと生成・検証を複製しない。

```cpp
t::TypedResourceBundle<Schema, t::HeapStorage> heap(manager, descriptors, sizes);
```

既定policyはこの段階ではHeapStorage。arena既定化は完了計画の順序11で行う。

## ResourceIdとcomponent

単独資源はPRIMARY、owned TextureはIMAGE／SAMPLERを別々に識別する。binding→ID→slot／offsetの順に計画する。同一componentを複数bindingへ接続できる。

```cpp
using Map = t::ResourceBindingMap<
    t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{42}, t::ResourceComponent::SAMPLER>,
    t::ResourceBinding<t::BindingKey{1, 3}, t::ResourceId{42}>>;
// binding 1:3 が単独Samplerなら、TextureのSamplerと同じ実体を使用する。
using Storage = t::ArenaStorage<Map>;
```

同一IDには物理kind・scope・作成infoの一致を要求する。Bufferはpayload型とdescriptor kindも一致させる。TextureのImageと単独SAMPLED_IMAGE、TextureのSamplerと単独SAMPLERは共有できる。sampled系とSTORAGE_IMAGEは既存descriptorのlayout指定が異なるため、同一IDの共有を拒否する。layout policyの追加は別課題。

`TypedBundlePlan::resources(scope)`と`resource_plan(scope,instance)`は、IDの重複を除いた異種初期化infoとCPU配置を返す。`placement(key,component)`のslotはこの異種一覧の番号。旧`buffers(scope)`はBufferだけの移行用一覧なので、mixed schemaではそのslotをindexに使わない。span／参照を返すplan APIは一時オブジェクトから呼べない。

`ResourceAllocationTotals::buffers`／`payload_bytes`は従来のBuffer数／payload量を表す。`images`／`samplers`も宣言とframe数から数え、ID共有後の実体数はplacement planで確認する。画像GPUメモリ量をこれらの値から推測しない。

external入力はowned slotを消費せず、元のDescriptorValueを保存する。追加の論理UseState・owner・部分Buffer範囲を維持する。external combinedは従来互換のPRIMARYによるopaque bindingとして扱い、そのkindはIMAGEを示す。ImageとSamplerの物理配置やID共有はこの表から生成しない。

## 所有と初期化

shared arena一つとframeごとのarenaを分ける。frame groupのinstanceは実際のframe index。ArenaOwnerは内部実体の借用参照を持ち、bundle／view／descriptorを所有しない。owning facadeはarena外に置く。split Textureはframe側で合成するため、shared側からframe arenaへの強い参照を作らない。`frame.sampler()`で保存したshared Samplerはframe Imageを保持しない。逆のshared Image＋frame Samplerも同様。

`frame.resource()`はdescriptor用のtyped facade、`frame.image()`／`sampler()`は各componentのowning accessorを返す。`buffer()`はBuffer専用に制約する。`initialize_shared()`ではshared componentを取得でき、frame用componentは取得できない。

`initialize_shared(callback)`は一度だけの同期初期化。GPU uploadはcallback内でsubmit／waitまで終える。返却時に全owned shared資源がidleでなければ失敗とし、bundleを以後利用できない状態にする。失敗前に記録したcommandの保持は、cancel／完了まで維持する。idle検査は画像contentの初期化やlayoutの正しさを証明するものではない。

記録ではImplの共通UseStateを登録し、ownerはcontrol blockで重複排除する。Typedのarena経路はCOMPLETION保持により完了／cancelでpinを回収する。既存ImageUpload等のSUBMISSION保持は現時点で残るため、保存した初期化Submissionが画像を保持する場合がある。この結果と保持の分離は次の順序4で扱う。

## 残る確保と次の単位

planはconstexpr対応のowned create-info variantだけを持ち、shared_ptrを含むResourceInitへは構築時に変換する。CPU領域へ置くのはImplとUseState。arena owner・CPURegion・rollback記録・vector／map、bundle／frame facade、descriptor／pipeline、Vulkan device memoryの個別確保は残る。heap用のcomponent一覧と旧Buffer一覧も生成時の管理データとして残す。

公開native accessor／DescriptorValueのshared_ptrは既存command・descriptorとの移行adapter。操作は保存済みImplへ直接届き、componentごとの世代ownerは追加していない。性能改善率は未測定。

次はCompletionの観測結果とin-flight保持を分離し、送信失敗・取消・完了の終端経路を統一する。descriptor layout／pool／setの配置はその後に行う。

## 検証

macOS arm64／Clang20／C++26、ASan／UBSan、MoltenVK／Vulkan validationで検証した。追加CPU5件・GPU10件により、componentのID共有・offset・scope、外部slot除外、ImageInfo配列、異なる作成info／kind／layoutの共有拒否、heap／arenaのupload→sampling→ImageReadback、両方向のsplit scope、commandだけの保持と再記録、GPU完了／取消回収、未完了共有初期化の拒否、外部論理制約、descriptor失敗時の回収と再試行を確認した。

VSL全749件中746成功／既知packed format3 skip、AHO GPU137件・CPU1070件、compile175件成功。変更C++10ファイル／7TUのstrict違反0。install済み公開ヘッダーだけで両policyのbundle利用をbuild／linkし、constexpr配置のconsumerを実行した。両Typedサンプルで3frame／12submission・UBO更新・resize・pipeline生成1回を維持し、PNG3枚がバイト一致した。

Windows／Linux実機、CPU並行操作、device lost、すべてのnative失敗位置の注入、性能比較は未検証。LeakSanitizerは無効化。backendのnative途中失敗／region再利用はResourceArenaの既存検証と共用し、ここでは新しいbundle／descriptorの失敗境界を追加で検証した。
