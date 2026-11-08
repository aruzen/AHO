# Fence／SemaphoreのCPU配置

順序8b後半。[同期使用状態](synchronization-state.md)の予約・完了観測は維持し、通常の名前付き生成と`ResourceArena`の生成を共通backendへ接続する。API名は移行中の仮案。

## 4責務と所有

| 責務 | 実装・契約 |
| --- | --- |
| storage | `ImplLayout`／`ResourceSlotLayout`の正確な寸法、`ResourcePlan`のID→slot→offset、`ArenaMemory`の領域管理 |
| operation | `initialize_fence`／`initialize_semaphore`、native配列の生成・逆順破棄・wait／reset。解決済みImplと保存native Deviceを使用 |
| lifetime | `synchronization_storage`が指定領域へImpl＋隣接contextを構築し、heap owner／arena rootがDeviceを保持。`synchronization_use`が予約・使用状態を検査し、明示native破棄の時点を判断 |
| sync | Fence要素別reservation／submitted ResultとSemaphoreのgraphics／WSI状態。完了証明は所有と分ける |

ImplはDeviceのraw参照、保存native Device、owner nodeと隣接contextの借用を持つ。contextは名前付きregistryへのweak参照だけを持ち、arena自身を強所有しない。heap ownerは正規Deviceを固定親として依存graphへ列挙する。通常のnative操作はregistry／owner探索を行わない。

`destroy()`は使用検査後にnative配列を回収する。CPUのImpl・context・Fence状態配列はroot寿命まで残る。root破棄ではImpl→context→slotの順に破棄し、Impl destructorは保存native Deviceで残りのnative handleを回収する。完了観測やregistryをdestructorから呼ばない。

## 配置と生成情報

`SemaphoreCreateInfo{count}`／`FenceCreateInfo{count, signaled}`をowned入力とする。countは1以上のuint32範囲。ResourceIdで同型の複数slotを区別する。構築順stage0に属し、失敗slotは局所rollbackし、成功済みslotだけをarenaの実構築順の逆順で破棄する。

寸法は実Implとcontext込みのslotからCMake compile-only probeで生成する。private headerと公開入力、compiler／target／構成／標準ライブラリABIをfingerprintへ含め、private実定義とのstatic_assertとartifact依存symbolで照合する。生成ヘッダーをinstallする。native handle配列・Fence状態配列・Semaphore追跡vectorのpayloadとVulkan／driver memoryは、このCPU一括領域に含めない。

## 利用例

通常の名前付き生成は従来と同じ。

```cpp
vsl::SynchroManager synchronization(device);
auto done = synchronization.create_fence("frame", false);
auto phase = manager.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, done);
phase.submit();
phase.completion().wait();
done.destroy();
```

arenaでは生成入力をplanに対応させる。

```cpp
namespace t = vsl::typed;
namespace s = vsl::storage;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 3> REQUESTS{{
    {{1}, s::ResourceKind::SEMAPHORE},
    {{2}, s::ResourceKind::SEMAPHORE},
    {{3}, s::ResourceKind::FENCE}}};
constexpr t::ResourcePlan<3> PLAN(REQUESTS, {t::LifetimeScope::FRAME, 0});
const std::array<l::ResourceInit, 3> inputs{
    vsl::SemaphoreCreateInfo{}, vsl::SemaphoreCreateInfo{}, vsl::FenceCreateInfo{}};
l::ResourceArena frame(device, PLAN, inputs);
auto phase = manager.start_phase<vsl::ComputePhase>(
    frame.semaphore({1}), frame.semaphore({2}), frame.fence({3}));
// wait側のSemaphoreは、別の送信／画像取得がsignalする必要がある。
```

外部入力にはHolderをそのまま指定する。import getterは元control block／Impl／UseStateを返し、importer ownerへ包み直さない。名前付きregistryは元contextに属する。別のregistryへ登録するAPIは今回追加しない。

## 記録・送信・取消・解放

Phase境界で正規ownerを検査し、WorkへCB単位で重複排除して保持する。wait／signal／Fenceのraw参照はそれぞれ別slotを指せる。同arenaなら3役のowner indexは同じでよいが、各実体の使用登録は独立して行う。記録／送信はraw Implのnative handleへ直接到達する。

元のarena／Holder／Typed viewを破棄しても、保存command・Phase・Workの保持契約で資源を生存させる。PENDING中のFence wait／reset／destroyとSemaphore destroyを拒否し、送信後は共通Resultを観測する。取消は予約を終端化し、画像取得後のcleanupは前段の予約・元sourceの移譲を使用する。

GPU完了時に`completion_resources`は解放されるが、再記録まで残す`resources`のSUBMISSION保持はslot置換／Domain破棄まで続く。Resultだけの保存はrootを保持しない。Presentation／DomainのWSI保持は別境界であり、graphics fence／Device idleによる厳密なpresentation完了は主張しない。

## 検証・残作業

同型slotの直接offset、context込み寸法・alignment・容量、途中失敗と同region再利用、heapの固定Device辺、importの正規ownerと共通状態、nativeだけの明示破棄を検証した。実GPUでは同Rootの別Semaphore 2個とFence・Pool・複数ring・Typed Bufferを使い、signal→wait→copy、元wrapper破棄、完了／取消後の回収と3役の1保持要素を確認した。再初期化は既存handle／Fence状態を変更する前に拒否する。

追加11件（CPU2／GPU9）と既存回帰を実行し、VSL926成功・既知packed format3skip、AHO GPU150／CPU1070、compile195成功。Vulkan検証レイヤーとASan／UBSanでエラーなし。strict規約検査は17ファイル／10TUで違反0、CMake323pathとMSBuild／filtersの新規3path一意も確認した。heap／arenaの描画・resizeは3frame／12submission／2draw、更新238831画素、1720×1080→2000×1360、Pipeline生成1回でPNG3枚が一致した。インストールした公開ヘッダーとライブラリのconsumer18targetが成功した。

途中失敗は、先行slotのnative生成後に後続slotの無効countを渡して検証した。実driverの生成失敗位置すべてやDeviceLostの注入、Windows／Linux実機、LSan、CPU並行利用、厳密なWSI完了と性能率は未検証である。

Debug／Release別の生成寸法とconsumer実行、x86_64向けcompile-only probeとstatic archive構築も成功した。15kindの計画はこの環境で1416 byte／alignment 8。構成の異なる生成ヘッダーとバイナリのlinkを拒否し、private定義をinstallしていないことも確認した。

今回も公開`data_`、Deviceの`shared_from_this` getter、Work／Result／reservation／WSI markerの個別確保は残る。次は8cのTyped frame配置・再利用、9の起動arena、10の厳密WSI完了、11の既定変更、12の旧入口廃止と測定へ進む。
