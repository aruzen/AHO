# Image／Samplerの共通backendと使用状態

2026-11-07。[段階的移行](arena-migration.md)の段階4を、共通backend（4a）、異種arena（4b）、Typedのschema接続（4c）へ分ける。本書の実装範囲は4a。Bufferで確定した、実体・物理UseState・所有境界の関係をImage／Samplerへ適用する。

## 責務と依存

| 配置 | 契約 |
| --- | --- |
| operation/image_ops・sampler_ops | 解決済みImplとLogicalDeviceImpl参照へnative生成／破棄を実行。領域の確保・shared owner・完了判断をしない |
| lifetime/image_storage・sampler_storage | 指定領域へImplと物理UseStateを構築。失敗時の巻き戻し、使用完了後の破棄、heapの所有境界を担当 |
| sync/ResourceUseState・Completion | 物理実体の使用・完了を観測。native資源やarenaを所有しない |
| storage | 既存ArenaMemoryを維持。Image／Samplerの公開寸法・混在offset・寿命群planは4bで追加 |

通常のImage／Samplerも指定領域への構築処理を使う。heap ownerの一つの割当にdevice所有、Impl、UseStateをまとめ、公開accessorはImplへのaliasing shared_ptrを持つ。ImplはdeviceとUseStateを借用し、自分のownerを所有しない。通常操作はImplに直接届く。

`operation::initialize_*` は途中で失敗したnative資源を回収する。`lifetime::construct_*` はC++実体を巻き戻す。`destroy_*` を呼べる時点の判断は所有者側にあり、native破棄関数は待機しない。private構築APIは未構築・十分な容量とalignmentを満たす領域を前提にする。未構築slotの取得・二重構築・overflowを含む利用者向けの検査は4bのplan／arena境界で提供する。

## 所有するimageと外部image

通常imageはImage・DeviceMemory・ImageViewを所有する。外部imageの経路はImageViewだけを生成・所有し、native imageとメモリを破棄しない。swapchain imageのheap ownerはswapchain世代も保持するため、公開imageが最後に解放されるまでnative imageの所有元が生きる。swapchainの内部へこのimage ownerを強く保持する参照は作らない。

既存ImageAccessorを外部入力として利用する場合は、そのImpl・物理状態・ownerを共有する。swapchainからの初期生成と、既存storageに追加のviewを作る操作は別の契約である。4bで既存storageから追加viewを作る場合も、native handleから独立した使用状態を作り直さず、元の物理状態を借用する。

ImageImpl／SamplerImplのデストラクタへnative破棄を隠さず、lifetime側からoperationを明示的に呼ぶ。imageの部分生成失敗では、view→image→memoryの順に回収する。外部imageではviewだけを回収する。

`ImageAccessor::get_device()` はdevice本来の共有所有を返す。deviceを取り出しただけでimage世代まで保持するaliasを作らない。

## 使用状態と資源保持

```cpp
#include <VSL/lifetime/resource_use.hpp>

vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST> image(device, width, height);
vsl::Sampler sampler(device);
auto image_use = image.resource_use(); // 物理ImageImplの状態とowner
auto sampler_use = sampler.resource_use();
```

Image／Samplerにつき物理UseStateは一つ。`use_state()` と `resource_use()` は同じ状態を参照する。ImageUpload、ImageReadback、copy、barrierはそれを登録する。RenderPassBeginも実際に選んだframebufferのattachmentを登録し、既にframebufferが所有する資源のowner保持は追加しない。

descriptorのnative binding表をBuffer専用から共通の `ResourceUseState* + owner_index` へ拡張した。Typed／通常VSLのどちらで更新しても、記録時のbindingに入っているBuffer／Image／Samplerを登録する。Typed imageの任意の追加Useは論理制約として共存する。combined image samplerは両方、配列は全要素を登録する。ownerの保持はcontrol blockで重複排除し、異なる物理状態は省略しない。

AHOのexperimental Textureも物理Imageの状態を返し、従来の更新・layout制約を追加論理状態として重ねる。TextureUpdateは両方の使用可否を確認し、VSLから直接記録したImageの使用も認識する。descriptorではnative表が物理状態を登録し、追加分だけを論理制約として扱う。

保存したcommandやTyped resourceは記録前の実体を所有する。記録後はlifetime側が必要なownerを保持し、sync側が完了を観測する。既存commandと通常descriptorのSUBMISSION保持は維持する。TypedDescriptorSetをCOMPLETION指定した経路は、GPU完了観測／cancel後に記録の保持を解放できる。保存したview・descriptor・command自身の所有は別に残る。

使用状態の統一は、自動的なlayout推論やアクセス競合の解決を追加するものではない。layout、barrier、queue間同期、CPU操作の直列化は従来どおり利用側が指定する。graphicsの完了観測とpresentationの完了は分ける。

## 互換性と次の単位

公開Image／Samplerコンストラクタ、SamplingMode／EdgeMode、ImageAccessor／SamplerAccessor、Typed resource、upload／readbackの使い方を維持する。private Implのdeviceは借用pointerへ変わり、独自にImplをnewして共有所有へ入れる方式には対応しない。privateヘッダーはinstallしない。

通常heap ownerと公開accessorのaliasing handleは外部所有境界として残る。Typedと既存descriptorがshared_ptr<Impl>を要求する接続は移行adapterでもある。descriptorのbindings／使用状態vector、native device memory等の個別確保は残る。性能改善率は未測定。

4bでは実定義からImageImpl／SamplerImplとslotのsize・alignmentを生成・検査し、Bufferを含む異種配置planと寿命群ごとのarenaを実装する。同型の複数slot、ResourceId／BindingKeyの分離、外部資源をslotに数えない規則、正しい逆順rollbackを維持する。4cでTyped image／sampler schemaとbundleへ接続する。swapchain image自体のGPUメモリをarenaが所有する設計にはしない。

## 検証

Clang20／C++26、macOS arm64／Apple M1、ASan／UBSan、MoltenVK／Vulkan validationで確認した。

- VSL720件中717成功、既知のpacked format3件skip。追加8件は指定領域の複数image／sampler、失敗後のslot再利用、借用native imageの生存、device取得による不要なimage保持の防止、native descriptor更新、Typed computeの読取結果と完了時回収、attachment使用、保存したcommandとcancelを検証する。
- AHO GPU137件・CPU1070件成功。追加1件でVSL経由の使用中のAHO更新拒否、物理状態と論理制約の区別、cancel後の再利用を確認した。
- Typed compile151件成功。変更C++33ファイル／22TUのstrict、最終テスト1ファイル／1TU、追加のAHO接続を含む4ファイル／5TUのstrictはすべて違反0。
- heap／arena両Typedサンプルで3 frame slot／12 submission／2 draw／238831 UBO変更画素／1720×1080→2000×1360 resize／pipeline生成1回を確認した。
- 再install後の公開APIだけでImage／Sampler、AHO arena／snapshot、Typed bundleのconsumerをbuild／linkし、CPU実行成功。private Implは非公開のまま。
- CMakeの263 source pathと、MSBuildの新規10 path／重複を検証した。Windows実行は未検証。

Image／Samplerの公開arenaと配置計算は未実装で、今回の指定領域構築テストはprivate backendの境界を直接検証している。Windows／Linux実機、device lost、CPU並行操作、native APIの全失敗箇所の注入、性能比較は未検証。LeakSanitizerは無効化し、validation・weak_ptr・実データの読取を併用した。
