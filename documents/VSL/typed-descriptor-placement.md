# Typed bundleのdescriptor配置

[完了計画](arena-completion-plan.md)の5cで、[descriptor論理計画](typed-descriptor-plan.md)と[所有依存graph](owner-dependencies.md)をTypedResourceBundleへ接続する。

## schemaと配置

`ArenaStorage`はBuffer／Image／Samplerに加え、宣言したSetのLayout／Pool／SetをResourceArenaへ配置する。set番号の穴は配置数に含めない。Layoutはshared群へ宣言数だけ配置し、shared Setを持つときだけその群のPoolを一つ作る。各frameは自分のSetが必要とするLayoutをsharedからimportし、Pool一つと実際のframe Setを配置する。

`TypedBundlePlan::arena_plan(scope, instance)`はdescriptorとimportも含む配置計画を返す。既存データ資源のID／slot／offsetを変えず、owned descriptorを後ろへ加える。descriptor用IDは、externalを含む既存IDを予約したうえで未使用の値から決定的に割り当てる。同じ資源を共有するbindingも予約の対象とし、明示的なUINT64_MAXを使っていても最大値＋1の採番は行わない。

`descriptor_layout(set)`と`descriptor_set(set)`は資源IDを返す。`descriptor_pool(scope)`はPoolがない群ではnulloptを返す。frameごとの実体は同じIDと異なるLifetimeGroup.instanceで区別する。

```cpp
using Bundle = vsl::typed::TypedResourceBundle<Allocation,
    vsl::typed::ArenaStorage<>>;
Bundle resources(command_manager, legacy_descriptors, sizes);
const auto& placement = resources.placement_plan();
const auto frame_plan = placement.arena_plan(vsl::typed::ResourceScope::PER_FRAME, 0);
const auto set_id = placement.descriptor_set(2);
const auto set_offset = frame_plan.placement(set_id).offset;
const auto cpu_bytes = placement.total_arena_bytes();
```

既存のresources／buffers／resource_plan／bytes／alignment／total_bytesは、データ資源だけを返す互換APIとして意味を維持する。全領域の見積りにはarena_plan／arena_bytes／total_arena_bytesを使う。領域確保時はarena_planのbytesとalignmentを組み合わせ、旧alignmentと新arena_bytesを混ぜない。サンプルのCPU領域表示も全領域の値へ切り替える。生成用vector／mapはconstexpr計画へ格納せず、論理計画のspan／個数からruntime scratchとして組み立てる。

空schemaは領域もPoolも必要としない。明示的な空Set、external資源だけを使うSetは、データ資源が0個でもdescriptor用のarenaを必要とする。

## 所有・利用・回収

arenaのSetはFromResource(COMPLETION)でTyped APIへ接続する。通常操作は解決済みImplと共通のSet使用状態を参照する。同じrootと固定親に属するbinding所有は借用へ正規化し、別rootの可変入力と追加ResourceUseはDYNAMIC方針で保持・循環検査する。

PlacedResourceBufferViewのdescriptorも、raw shared_ptrを経由せず、owner_refからImplへのaliasを返す。Typed入力の使用状態を省略できるのは、物理State*とcontrol blockが一致し、明示ownerも空または同じcontrol blockの場合に限る。同じ物理状態でも別owner、null alias owner、別controlのstateを伴う入力は追加制約として保持し、graph検査へ渡す。

frame rootはimportしたLayoutのshared rootを保持する。shared facade自体や他frameのfacadeをrootへ格納しない。Setだけを外へ保持しても必要なLayoutとbindingは残り、無関係なframeは回収できる。shared Setへframe資源を接続すると逆辺になるため拒否する。公開view・保存command・記録・送信の保持境界と、完了結果の観測は従来の分離を維持する。

構築途中の失敗は、生成済みのSet／Pool／Layoutとデータ資源をResourceArenaの逆順破棄で回収する。初期bindingの検証失敗も同じ経路を使う。配列writeは全要素を検証してからnative更新するため、配列内の一部だけを更新したとは扱わない。

HeapStorageは従来のManagerによる割当を維持する。ArenaStorageのManagerInterface引数は同じdeviceかを検査する互換入口で、Layout／Pool／Setの生成には使わない。この引数とdata-only計画入口の整理は旧API廃止段階に残す。Typedの既定policyを切り替えるのは段階11である。

## 残る確保と検証範囲

CPU arenaにはImplと物理状態を配置する。Set wrapperのResourceUseGroup、frame／shared facade、bindingコンテナ、scratch vector／map、Vulkan device memoryは別の確保である。性能率は未測定。

CPUでは既存配置不変・ID衝突・set番号の穴・空Set／空schema、GPUでは実offset・Layout共有・frame別Pool／Set・固定依存・共通状態・完了／取消回収・部分接続後の失敗を確認する。AHOでは旧storageのexternal viewと、再確保後にも共通のlogical制約を利用する経路を検証する。

追加CPU3件、VSL GPU4件とAHO GPU1件が成功し、既存配置テストでも実際のSet／Pool／Layout offset、共通owner、Pool予算を確認した。初回の逆cycle検証でPlacedResourceBufferViewのprovenance消失を再現し、前述の修正後は物理入力、別owner、null alias owner、別controlのstate、論理入力のすべてが循環として拒否され、両rootが回収された。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL817件中814成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約175件、strict11C++／9TU違反0、CMake280パス検査が成功した。installed public consumer8targetのbuild／link／CPU実行とprivate定義非公開を確認した。新公開APIのconstexpr計画とheap／arena両テンプレート経路は、Debug／Release／x86_64でもcompileした。Impl寸法と生成fingerprintは今回変更していない。

heap／arena両描画は3frame・12submission・2draw・238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回を維持し、PNG3枚が一致した。サンプルのarena CPU領域表示はdescriptorも含む1520 byteとなる。Root／facade／コンテナとGPUメモリはこの値に含まない。VUID／sanitizer errorはない。

独立レビュー2名の指摘を反映した。Bundle全体でnative allocatorの全失敗位置を注入した検証、Windows／Linux実機、CPU並行、実DeviceLost、LSan、性能率は未検証。native backendの既存失敗テスト、低位arenaのrollback、Bundleでの初期接続失敗とowner参照数復帰を組み合わせて確認している。
