# Bufferのstorage世代・アクセス・識別・寿命群

2026-11-02。PR #317のレビューで指摘された4点を、他の資源のarena化とschema自動展開より先に修正する。コード名は引き続き仮案。[配置実装](buffer-placement.md)の現行契約を補う。

## 同じstorage世代は同じ使用状態

BufferImplごとに一つのResourceUseStateを構築する。通常のBufferAccessor、BufferView、TypedVertexBuffer／TypedIndexBuffer／TypedResourceBuffer、外部Bufferから作るTyped viewはこの状態を参照する。Typedの所有Bufferが別の_useを確保する経路は廃止した。

`BufferAccessor::use_state()`は借用、`resource_use()`は外部保持用のaliasを返す。ResourceUseのstateとownerは物理storageの状態と所有者。追加のconstraint_stateとconstraint_ownerは、AHOの論理Buffer全体の更新制限とCustom資源等の所有者を保持するための一層である。既に同じstorageへ接続したResourceUseを再度importしても状態を作り直さない。別storageへ接続済みの複合制約を論理制約として重ねる操作は拒否する。

ResourceUseは公開view・binding・commandの境界で使う。arena内部にowning ResourceUseを格納しない。submissionは実体と追加の所有者を保持し、使用状態はsubmissionをweak参照する。AHOの論理状態自体をsubmissionで強く保持する必要はない。

通常VSLのCopyBuffer、頂点／index bind、buffer-image間copy、descriptor bindも同じ状態へ使用を登録する。descriptorには、既存のbinding ownerを借用するBufferImpl参照とowner位置を保存し、記録時に使用を登録・所有者を保持する。TypedBatchのbind省略経路も同じ登録を行う。

自動的な登録・保持にはPhaseのrecording submissionを使う。Phase外でcommand.invokeした後にnative submitする旧経路は、完了と保持を対応付けられないため、記録可能なPhaseを要求する。製品のpicking readとテスト用readbackは独立managerのComputePhaseへ移し、submit／完了待ち／read_intoを明示した。Vulkan APIを直接記録・送信するescapeの同期と所有は利用側が管理する。

通常Bufferのcopy／mapping取得、既存holderのcopy／write／flush／invalidate、新しいviewのwrite／read_intoは共通状態を確認する。したがって、一方の入口で記録した使用を、他方の入口での更新検査も認識する。PENDING中は拒否し、使用を登録した全submissionの完了または取消後に再開できる。

AHOは論理状態に加え、現在のnative storageの状態もbusy／wait／更新検査へ合成する。再確保すると新しいstorageには別の状態ができ、旧viewは旧世代の状態を参照し続ける。旧AHO viewの使用は、追加の論理状態によって現在のAHO Buffer全体の変更も制限できる。

## 読み取りは呼出し内で完結させる

新しいBufferRef／BufferViewのreadと、PlacedBufferViewのread_bytesは削除した。ownerを保持したspanでもアクセス競合は防げないため、read_intoへ置き換える。

```cpp
std::array<std::uint32_t, 4> result{};
placed_view.read_into(result); // レコード数・idle・範囲検証、invalidate、コピーまで完了
// 以後GPUが元Bufferを書き換えても、resultは呼出側が所有する独立した値。

std::array<std::byte, 16> bytes{};
native_buffer.read_into(bytes); // BufferAccessorも同じ処理を利用
```

部分読み取りは先にview(first, count)を作る。Typed版は型とレコード数、byte版はoffsetと容量を検査する。失敗時に利用側へnative mappingのspanを返さない。

今回のCPU側の契約は直列操作。複数threadからのrecord／host accessの並行利用は保証しない。read_intoは呼出し内で検査とコピーが終わるため、呼出し間にGPU記録を挟んでも、保存した結果へのアクセス競合は生じない。

旧LocalBufferHolder::data／read、AHOの旧map／read等のraw pointer／spanは、互換用の明示的なescapeとして残る。取得時やhelper呼出時のidle確認は、返されたpointer／spanを後で参照する期間まで保護するものではない。利用側が使用期間全体を同期する必要がある。安全な直接アクセスAPIは未提供で、将来MappedReadAccessを追加する際はownerとhost leaseを保持し、lease終了まで共通状態からGPU記録を拒否する。名前だけを変えたspan返却をその代替にしない。

## ResourceId・BindingKey・Slot

| 要素 | 意味 | 実装 |
| --- | --- | --- |
| ResourceId | schema内のどの資源か | uint64値を持つ独立した型。binding番号やアドレスを含めない |
| BindingKey | shaderのどの場所か | set／binding／配列要素。frame番号は含めない |
| ResourceBinding | 接続先が参照する資源 | BindingKey → ResourceId。複数keyが同じIDを参照できる |
| BufferPlacement | CPU実体を置く場所 | ResourceId → slot番号／offset。planがschema順から導出 |

```cpp
namespace t = vsl::typed;
constexpr t::ResourceId CAMERA{10}, TRANSFER{20};
using Arena = t::TypedBufferArena<
    t::BufferResource<CAMERA, Camera, uniform_usage>,
    t::BufferResource<TRANSFER, std::uint32_t, transfer_usage>>;
using Bindings = t::BufferBindingMap<
    t::ResourceBinding<t::BindingKey{0, 0}, CAMERA>,
    t::ResourceBinding<t::BindingKey{2, 7}, CAMERA>>;

Arena arena(device, Arena::Plan({1, 32}));
auto camera = arena.view<CAMERA>();
auto alias = arena.binding_view<Bindings, t::BindingKey{2, 7}>();
// cameraとaliasは同じImpl。bindingを二つ宣言してもImplは増えない。
auto transfer = arena.view<TRANSFER>(); // descriptorに属さない資源にもIDを使える。
```

旧BufferSlot<set, binding, ...>とview<set, binding, ...>は上記へ変更する。PR未統合の新APIなので、資源IDへbindingを埋め込む互換aliasは残さない。

ResourceIdはグローバルなruntime registryではない。同じ宣言IDを各frame instanceへ展開した場合も、具体的なstorage世代は別のImplと使用状態を持つ。plan内の重複ID、重複BindingKey、未宣言ID／keyの参照はコンパイル時に拒否する。binding mapは接続先からのID解決であり、既存ResourceAllocationSchemaやdescriptor writeを自動生成する機能はまだ含まない。

## 配置領域は寿命群で分ける

一つのTyped arenaには一つのLifetimeGroupだけを配置する。群はLifetimeScopeとinstanceの組で、同じ群の中は引き続き一括破棄する。複雑な部分解放は導入しない。

- SHARED: 長寿命の共有資源。instanceは0。
- FRAME: frame slotごとに別arena。instanceはframe slot番号。
- TRANSIENT: 処理／送信の区切りごとのarena。instanceは利用側が与える識別値。

BufferResourceのscopeが異なる宣言を一つのplanへ入れるとコンパイルエラーになる。FRAMEのinstanceはplan単位なので、frame 0とframe 1の資源を一つのplanへ混ぜることもない。

```cpp
using Shared = t::TypedBufferArena<t::BufferResource<CAMERA, Camera, uniform_usage>>;
using Frame = t::TypedBufferArena<
    t::BufferResource<TRANSFER, std::uint32_t, transfer_usage, t::LifetimeScope::FRAME>>;
Shared shared(device, Shared::Plan({1}));
Frame frame0(device, Frame::Plan({32}, 0));
Frame frame1(device, Frame::Plan({32}, 1));
```

各instanceは別のCPU領域・ArenaOwnerを持つ。shared viewはshared arenaだけを保持する。異なる群の資源を同じcommandで使う場合は、submissionが各ownerを保持し、完了後は各群を独立して回収する。ArenaMemoryを明示する場合も、同時に生存する各群には別領域を渡す。既存claim検査で同じ領域への二重構築を拒否する。

低水準のstorage::BufferArenaへ手動でBufferCreateInfoを渡す場合も一つの寿命群として扱う。runtimeの群IDの重複を全世界で監視するregistryや、低水準CreateInfoから寿命を推測する機能は作らない。

## 検証と次の範囲

追加ケースは、旧Typed／通常VSL／新viewの状態identityと相互更新拒否、読み取り結果の独立、異なるbindingの同じ実体への接続、shared viewを残した状態でのframe arena回収、AHOの旧storage snapshotと追加の論理制約を確認する。混在寿命・重複ID／key・未定義ID／key・旧span API・型違いの読み取りはcompile契約で拒否する。

既存ResourceAllocationSchemaからの自動展開は、このResourceId・接続map・寿命群への変換として後続で実装する。descriptor内部のborrow／外部owner分離、MappedReadAccessのhost lease、device lost時の回収方針は引き続き未実装。image／descriptor／pipelineのarena化へはまだ展開しない。性能比較は未測定。

2026-11-02の検証結果（macOS arm64／M1／Clang20、ASan・UBSan、MoltenVKとVulkan validation）:

| 検証 | 結果 |
| --- | --- |
| VSL全体 | 690成功、環境依存のpacked format 3 skip。今回の新規5件と従来のarena 9件を含む |
| AHO GPU／CPU | GPU 121成功（今回2件追加）、CPU 1070成功 |
| compile契約 | 128成功。arena契約は旧7件から13件へ拡張 |
| strict規約 | 変更C++ 29ファイル／27 TU、違反0 |
| 既存Typed描画 | 3 frame slot、12 submission、2 draw、UBO更新、resize、pipeline生成1回をverifyで確認 |
| installed consumer | 移動済みprefixの新しい公開ヘッダーとbinaryでbuild／link／CPU検証成功。private Implは非配布 |
| ABI不一致 | 同じsize／alignmentを持つ旧artifactの生成定数を現binaryへ混ぜ、ABI固有symbolのlink失敗を確認 |
| multi-config／cross | Debug／Releaseの実寸法assert成功。x86_64のcompile-only probe成功、archive内objectがX86_64であることを確認 |

Windows／Linux実機、x86_64 binaryの実行、並行CPUアクセス、device lostは未検証。LeakSanitizerは使用せず、ASan／UBSan・validation・weak owner失効・既存のrollback／返却回数テストで寿命を確認した。
