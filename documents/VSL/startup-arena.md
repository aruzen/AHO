# 起動資源の配置と寿命群

順序9e。Instance／PhysicalDevice／Surface／Deviceの共通backendを、Device引数不要の公開`lifetime::StartupArena`から利用する。AHOの起動・追加Windowにも接続し、順序9の受入検証を完了した。順序10のSwapchain／presentation配置と厳密完了は対象に含めない。

## 配置と入力

`storage::StartupPlan<capacity>`は`StartupRequest{ResourceId, StartupKind, external}`からslot／offset／bytes／alignmentを計算する。capacityは宣言数の上限、実数は入力spanのsize。同型のImplも別ResourceIdとslotで区別する。外部資源は領域を消費しない。Device前提のResourceArenaへ起動資源kindを混ぜず、既存19型の生成済み寸法とartifact guardを使う。

`lifetime::StartupArena(plan, inputs[, ArenaMemory])`は計画ごとに一つのCPU領域とownerを作る。`size()`は外部資源を含む宣言件数、`slots()`は配置した実体の数。getterは所有する通常VSL facadeを返し、取得後の操作は解決済みImplへ直接届く。配置表の検索はgetter時だけである。

| owned kind | 初期化値 | 永続依存 |
| --- | --- | --- |
| Instance | InstanceCreateInfo | なし |
| PhysicalDevice | PhysicalDeviceInit{InstanceSource, optional index} | 元Instance |
| Surface | SurfaceInit{InstanceSource, OSWindowOwner} | 元Instance、元OS window |
| Device | DeviceInit{PhysicalDeviceSource, DeviceCreateInfo} | 元PhysicalDevice |
| graphics Device | GraphicsDeviceInit{PhysicalDeviceSource, SurfaceSource, validation} | 元PhysicalDevice。直接Surfaceは生成中だけ |

Sourceは同じ計画のResourceIdか、外部の所有するfacade。外部slotの初期化値も通常のVSL facadeを受け取り、元control blockを維持する。非所有alias／別CBへ付け替えた起動資源／退役・無効実体は受理しない。任意native handleの暗黙adoptは行わない。

## 既定の寿命群

AHOでは次の3群を別ownerにする。

1. 共通Instance。
2. 選択したPhysicalDevice＋Device。共通Instanceを外部参照する。
3. WindowごとのSurface。共通Instanceと、plugins／Engineを含まないOSWindowOwnerを保持する。

Device生成のSurfaceはqueue選択中だけの入力で、入力snapshotの破棄時に保持を外す。Deviceだけを必要とするBufferが残っていても、Surface群／OS windowは回収できる。Swapchainは従来どおりDeviceとSurfaceの両方を保持し、その世代の回収時点はsync／lifetimeで管理する。

同じplanへInstance／Surface／Deviceを明示して配置することもできるが、一括破棄を選択したことを意味する。依存辺を減らしても同じowner内の寿命は分かれない。共有資源と短命なWindowを自動で同居させない。

```cpp
using namespace vsl;
const std::array requests{
    storage::StartupRequest{{1}, storage::StartupKind::PHYSICAL_DEVICE},
    storage::StartupRequest{{2}, storage::StartupKind::DEVICE}};
const storage::StartupPlan<2> plan(requests);
const std::array<lifetime::StartupInit, 2> inputs{
    lifetime::PhysicalDeviceInit{instance, std::nullopt},
    lifetime::GraphicsDeviceInit{typed::ResourceId{1}, surface, VALIDATION}};
lifetime::StartupArena devices(plan, inputs);
auto device = devices.device({2});
// devicesを破棄してもdeviceが群全体を保持する。surfaceはこの群に残らない。
```

headlessの場合は`DeviceInit{physical_source, DeviceCreateInfo{}}`を使う。Instanceの入力は通常`InstanceCreateInfo`で、GLFWを必要としない。Window向けには`glfw_instance_info(info)`がlease中に必要拡張を所有文字列へコピーし、validation設定を補う。Instanceの永久ownerへGLFWを追加しない。

## 構築・公開・回収

公開constructorは入力variantを一時vectorへsnapshotする。生成中のlogger／native callbackが元入力を変更しても、後続の入力とcall-only依存を保持する。Rootにはこのvectorを保存せず、構築・公開後に破棄する。

初期化値の型・ID・外部ownerを検査し、管理配列を確保してからCPURegionをclaimする。宣言順とは独立にInstance→PhysicalDevice→Surface→Deviceで構築する。親はraw参照として受け取り、未公開の同rootへ強所有を作らない。失敗時は成功した実際の順を逆にたどり、native実体／C++実体を破棄してからclaimを返す。claim失敗側が既存arenaのclaimを返すことはない。

全構築成功後はRootのnodeを公開し、owned Impl全部へ同じnodeを設定してから、元親のweak contextを接続する。外部資源のnode／contextは変更しない。固定依存は既存ownerをcontrol block単位で重複排除して保持し、getter用weak参照と区別する。通常の操作でgraphを探索しない。

最後の外部保持が外れるとnodeをretireし、成功済みslotを依存逆順で破棄、CPURegionを返却、固定親を解放する。CPU Implの一括配置は、slot外vector／候補scratch／所有control block／Vulkan device memoryの一括確保を意味しない。外部ArenaMemoryの`shared_ptr<void>`は既存opaque契約で、その内部に隠れた依存までgraphが検査する保証はない。

## AHOと互換入口

StandardEngineと追加WindowのSurface生成はStartupArenaを使う。候補一覧はscratchで、勝者のPhysicalDeviceだけをDevice群へ構築する。設定indexと既存の選択例外を維持する。EngineDataは既存6fieldのままで、arenaのgetterによるaliasが各群を保持するため、arena本体の二重保存は不要である。

`Vulkan(VulkanAccessor)`は旧EngineDataのVulkan fieldへ配置済みInstanceを接続し、`Surface(PureWindow*, Surface)`は既存plugin登録へ配置済みSurfaceを接続する移行adapterである。native資源は再生成しない。後者は元のmanaged OS ownerとlogical-liveを検査する。生成callbackでWindowがcloseされた場合は、boot／追加Windowの公開直前にもData／OS identityを照合し、既定Engine登録や描画状態をcommitしない。

通常VSLの旧heap constructorも同じbackendを使い続ける。公開data_／sharedImplと旧facade adapterは順序12で廃止する。WSIの厳密presentation完了、退役Swapchain復旧、0 extent、Typedのarena既定化は後続であり、9eで全体完了とはしない。

## 検証

VSL10件・AHO3件を追加し、constexpr配置と入力条件、逆順宣言・同型複数slot、外部元owner、成功Deviceの後で別Deviceを失敗させたrollbackと同領域再使用、viewによるclaim保持、公開facade破棄後のTyped command、call-only Surface解放、managed plugin登録、AHOの三群分離／閉じたWindowの独立回収／複数Windowを確認した。

- 追加VSL10件・起動関連AHO13件、重点VSL125件／AHO46件、全VSL1,022件・AHO GPU157件／CPU1,070件、コンパイル契約195件が成功。
- Apple M1／MoltenVK1.4.2でvalidation＋ASan／UBSanを使用。従来packed integer clearのdriver制約3件はskip、LSanは無効。
- 規約15ファイル／8翻訳単位と最終テストの補足検査で違反0、製品source354件違反0、MSBuild／filtersの新4pathは各1登録。
- install consumer24target、Debug／Release、x86_64 compile-only／archive、artifact混在link拒否、temporary placements()等の借用拒否が成功。
- heap／arenaは3frame／12submission／2draw、resize1720×1080→2000×1360、Pipeline生成1回、PNG3枚が対応ごとに一致。

他OS、実driver OOM／device lost、生成中closeを狙ったnative故障注入は未実施。slot外の確保量や性能率も未測定である。WSIの厳密証明は順序10へ残し、device idleをpresentation完了の証明とは扱わない。
