# 4責務とTyped arenaの移行

2026-11-08追記: 以後の最終範囲と実装順序は[完了計画](arena-completion-plan.md)を参照。Instance／Deviceの配置、Typedのarena既定化、旧公開入口の廃止まで対象とする。以下は既存の実装経緯を含む。

2026-11-06。Bufferの4責務分離が統合されたmaster `6b1f0166`を基点とする。[Bufferの契約](buffer-generation-contracts.md)と[責務の配置](buffer-layer-layout.md)を維持する。

## 方針と段階

既存APIとheap経路を併設し、arenaは明示選択する。Impl寸法生成はVSL全体で必須を継続する。通常VSLと配置経路は同じoperationを使い、内部借用と外部所有を区別する。性能向上率は未測定。

| 段階 | 実装単位と終了条件 |
| --- | --- |
| 1 | schemaからResourceId・binding map・寿命群別Buffer planを生成。型・scope・容量の矛盾を確保前に拒否 |
| 2 | bundleのarena policyとTyped viewを接続。通常descriptor／commandから使用でき、完了・取消で回収 |
| 3 | AHOの生成へ明示選択で接続。再確保後の旧view・共有alias・失敗時の旧世代維持を保証 |
| 4 | Image／Samplerの共通operation・所有・同期を分離し、異種Impl配置へ拡張。swapchain imageは外部借用 |
| 5 | descriptor layout／pool／setを配置。内部refと外部owner依存を分離し、記録時binding・更新制限・bind省略を維持 |
| 6 | shader、pipeline layout、render pass／pipeline、framebufferの順で展開。生成scratchと永続実体、互換resizeを区別 |
| 7 | 完了結果とin-flight保持を分離し、command・frame・swapchain世代へ展開。presentation側の完了をgraphics fenceと同一視しない |
| 8 | 内部vector／map・control block・保持表の確保数、保持メモリ、時間を計測し、容量指定と再利用を必要箇所へ追加 |

Device／Instanceは当面、外部の所有境界として保持する。CPU arenaにはImplと使用状態の本体を含め、内部コンテナやVulkan device memoryは別に確保する。資源の生存中にarenaを移動・拡張しない。新しい製品ディレクトリは作成前に利用者へ確認する。

## A: schemaのBuffer配置計画

`TypedBundlePlan<Allocation, BindingMap>`は既存`ResourceAllocationPlan`から次を計算する。

- bindingごとのResourceId、scope、外部資源かどうか、寿命群内のslot。
- shared群と一つのframe群それぞれのBufferCreateInfo、CPU bytes・alignment。
- shared一つと全frame群のCPU領域合計。frame数・payloadのruntime要素数は入力planからコピーする。

descriptor配列の各要素は、既定で異なる資源となる。自動IDは宣言順の空き番号で、明示IDとの衝突を避ける。IDは永続化用の番号ではない。`ResourceBinding<BindingKey, ResourceId>`の明示mapを渡すと、複数bindingを一つのowned Bufferへ接続できる。

同じIDには同じPayload型、descriptor kind、scope、要素数が必要。違う型が同じsizeofでも共有を認めない。外部資源はslotを使わず、外部資源をowned資源と同じIDへまとめる指定も拒否する。外部資源の共有は同じowning viewを複数のexternal inputへ渡して表す。

```cpp
using Map = t::BufferBindingMap<
    t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{42}>,
    t::ResourceBinding<t::BindingKey{1, 2}, t::ResourceId{42}>>;
t::ResourceAllocationPlan<Allocation> logical(3, sizes);
t::TypedBundlePlan<Allocation, Map> placement(logical);
auto shared_bytes = placement.bytes(t::ResourceScope::SHARED);
```

`ResourceAllocationPlan::totals()`は従来の宣言数の意味を維持する。共有IDによって削減された実体数は`TypedBundlePlan::buffers(scope).size()`、CPU領域は`total_bytes()`から確認する。payload bytesとCPU Impl bytesを混同しない。

空schema・外部資源だけの群は0 byte・alignment 1。runtime payload数はImplの個数ではない。SHAREDとPER_FRAMEは独立したarenaへ配置する。TRANSIENTのbundle自動生成は今回追加せず、既存の明示TypedBufferArenaで扱う。

## B: bundleと保持の接続

```cpp
using HeapBundle = t::TypedResourceBundle<Allocation>; // 従来の既定
using ArenaBundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<Map>>;
ArenaBundle bundle(manager, descriptors, sizes);
bundle.initialize_shared([](ArenaBundle::Shared& shared) {
    shared.buffer<0, 0>().copy(values);
});
auto phase = manager.start_phase<vsl::ComputePhase>();
bundle.update(phase, [&](ArenaBundle::Frame& frame) {
    frame.buffer<2, 1>().copy(frame_values);
});
phase << bundle.get(phase).bind_resources(pipeline).dispatch(4);
phase.submit();
phase.completion().wait();
```

上のbinding番号とpayloadは利用側のAllocationに合わせる。heapとarenaで共通のbundle構築・共有初期化・frame更新・descriptor接続を使用する。arena版の`buffer()`は`PlacedResourceBufferView`を返す。`copy`、`read_into`、部分`view`、`size`、`descriptor`を持ち、backendは解決済みBufferRefとownerを持つBufferViewである。旧backendの`.data_`や直接mappingが必要な利用側は、新policyへの移行時にview APIへ切り替える。共有Bufferとconst frameから直接copyできない型契約は維持する。

各非空群は一度だけCPU領域を確保する。群内の各slotをゼロ初期化し、同じIDの別bindingもその初期値を共有する。宣言・device limitを検証してから構築し、後続のdescriptor／frame構築に失敗した場合も、構築済みのownerを通して巻き戻す。

公開viewとbundle中のviewはarenaを保持するが、CPU領域内のImplはarenaを所有しない。frameからsharedへ所有が向く一方、sharedからframeへの参照は作らない。一つのshared viewはframe arenaを保持しない。

`ResourceRetention::SUBMISSION`は従来どおりSubmission所有者の寿命まで保持する。`COMPLETION`はGPU完了観測／記録取消でlifetime側のpinを解放する。arena policyはframeの保持、Typed descriptor、外部資源の追加論理制約をすべてCOMPLETIONへ接続する。heap policyの保持契約は変えない。資源自体が持つ使用状態はどちらのpolicyでも共通で、同期の新しい状態を増設しない。

commandを生存させている場合はcommand自身の保持も残る。GPU完了だけで、利用者が保存したviewやcommandまで無効化しない。native descriptorへの接続にはaliasing shared_ptrを使う移行adapterが残り、descriptor本体のarena化は段階5で扱う。

## C: 実利用

`aho-typed-example`はheapのまま、同じscene／shader／verifyを使う`aho-typed-arena-example`を併設する。両方とも3 frame slot、shared／frame UBO、TypedBatchの2 drawとbind省略、resize時のpipeline再利用を検証する。

```sh
cmake --build build --target aho-typed-example aho-typed-arena-example
./build/bin/aho-typed-example --verify ./build/typed-heap-verification
./build/bin/aho-typed-arena-example --verify ./build/typed-arena-verification
```

残る個別確保はframe wrapper、descriptor／layout／pool、shared所有境界、使用状態内のvector等。VulkanのBufferとdevice memoryもBufferごとに確保する。新しい一括領域がこれらを全て含むとは扱わない。

## 今回の検証と次の境界

Clang20／C++26、macOS arm64／Apple M1、ASan／UBSan、MoltenVK／Vulkan validationで検証した。

- VSL712件中709成功、対応外packed format3件skip。新規はCPU配置契約5件とarena bundleのGPU／寿命契約9件。
- AHO GPU121件成功。Typed compile137件＋AHO TypedBuffer8件の145件成功。heap／arena両方のbundle正常・負例15件ずつと、非trivialな新viewの拒否も含む。
- 変更C++16ファイル／13TUのstrict規約検査と、新viewヘッダー1ファイル／1TUの補完検査が違反0。
- heap／arena両サンプルで3 frame slot／12 submission／2 draw／238831 UBO変更画素／resize 1720×1080→2000×1360／pipeline生成1回。保存した3枚のPNGは両経路でバイト単位に一致。
- installed consumerで公開ヘッダーからbundleの構築・更新経路をbuild／linkし、CPU上の配置計算も実行。異なるartifact IDを持つ同寸法headerとの混在はlink guardで拒否。private Implはinstallしない。
- CMakeの関連targetとMSBuildのXML／新規path／重複登録を確認。生成済み寸法情報とABI定義はCMake targetから受け取る。手書きvcxprojだけで生成処理が完結する対応は追加していない。

Windows／Linux実機、device lost、並行CPUアクセス、性能比較は未検証。CPUアクセスは直列化する。LeakSanitizerは無効化し、weak_ptrの失効と既存の破棄検査を併用した。

今回の到達点は段階1・2と、その実利用検証A〜C。次は段階3のAHO storage世代への接続である。Image／descriptor／pipelineのarena化、Impl集合ヘッダーの資源別分割、全Completionの結果／保持分離はまだ完了していない。

## 段階3の追記

D1〜D3で[AHOのsnapshot・固定import・可変arena世代](../AHO/arena-buffer.md)を接続した。live bindingと世代snapshotを分け、固定importは容量内だけ、可変Bufferは新世代を別arenaへ作る。旧snapshotが新世代を保持し続けないよう、論理UseStateも現在値の所有から分離した。次は段階4のImage／Samplerである。既定heap API、寸法生成の必須方針、CPU ImplとGPUメモリの区別は維持している。

11月7日のレビュー修正で固定importと可変Bufferの公開継承を除去し、snapshotのviewからだけ明示的にimportする契約へ揃えた。factory返却viewのarena ownerもそのまま保存し、Bufferごとのgeneration wrapperによる保持重複を除去した。factory設定は論理状態に同居させ、実体の破棄依存は返却ownerが保持する。Image等へ展開する前提として、物理世代・論理状態・arena ownerの識別を混ぜない。

## 段階4の分割

段階4を、4a: Image／Samplerの共通backendと物理UseState、4b: 実寸法生成と異種arena、4c: Typed schema／bundle接続の順へ分ける。先に[通常APIと指定領域構築の共通処理](image-sampler-backend.md)を確定し、後の配置計画がnative生成や検証を複製せず利用できるようにする。

4aではImage／Samplerのoperationとlifetimeを分離し、device／swapchain所有をImpl外へ置く。通常API・Typed／native descriptor・転送／barrier・framebufferの全経路で同じ物理使用状態を登録する。外部imageはviewだけを所有する。Image／Samplerを利用者向けarenaへ配置するAPI、公開寸法・offset生成、schemaの自動展開は4b以降の終了条件に残す。

4bの[ResourceArena](resource-arena.md)と4cの[Typed owned Image／Sampler](typed-owned-resources.md)で、異種配置とcomponent単位のschema接続を実装した。現在の順序は[完了計画](arena-completion-plan.md)を参照。次は完了結果とin-flight保持の分離であり、descriptor以降の配置とDevice／Instanceは引き続き未完了。
