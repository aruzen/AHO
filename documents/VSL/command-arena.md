# CommandPool／CommandBufferの共通backendとCPU配置

[完了計画](arena-completion-plan.md)の8b前半。[外部Domain](command-domain.md)の上に、通常生成とResourceArena配置を接続する。Fence／Semaphoreの配置・使用予約とTyped frameの展開は後続である。

## 4責務と実体

operationはPool生成／破棄、native Buffer群の割当／解放、reset／begin／endを受け持つ。解決済みImplまたはnative handleを直接受け、ownerやDomainを探索しない。通常生成とarenaで生成・検証を複製しない。

lifetimeはslot上のCPU構築／破棄、heap root、Pool固定親、getter用weak metadata、Domainの接続と終了を管理する。syncのCompletion観測を利用して公開resetの可否を判断する。storageはCPU領域と正確なslot配置を扱う。

PoolImplはDeviceへの借用と保存native Device／Pool、owner nodeを持つ。BufferImplはPoolへの借用、保存native Device／Pool、native Buffer配列、現在slot、owner node、隣接OwnerContextとRecordingContextへの借用を持つ。ImplからDevice／Pool／Domain／Workを強所有しない。

Bufferの隣接OwnerContextは元のPool ownerとDomainのweak参照を持つ。通常の記録・retainはImpl内の解決済みRecordingContextへ直接届き、getter／接続だけがOwnerContextを利用する。heap rootはBuffer→Pool→Deviceの固定親をvisitorへ列挙する。同arenaのPoolは借用に正規化する。

## 配置情報と確保範囲

VSLはCOMMAND_POOL_IMPL／SLOT、COMMAND_BUFFER_IMPL／SLOTのsize・alignmentをprivate実定義から生成する。本体とcontext込みslotの両方をactual static_assertで照合し、standard-layoutなaligned byte配列の先頭offset0を検査する。GPU ResourceUseState用の追加slotは作らず、USE_OFFSETはNO_SEPARATE_USEである。

CMakeのcompile-only probe、構成別header、cross compile、入力hash／artifact guard、installへ接続する。公開Command APIとprivate slot定義もhash対象である。Impl定義を利用者へinstallしない。

このarm64構成ではPool本体／slotとも32 byte、Buffer本体80 byte／slot120 byte、alignmentはいずれも8である。13種を一つずつ置く計画は1192 byte・alignment8になる。他の対象は生成値を利用する。

CPU一括領域に含むのはImplと隣接OwnerContextである。native Buffer数は初期化時のcountで決まる。一つのring Implが複数native Bufferを持ち、そのvectorとnative driver内のcommand memory、外部Domainのcontext／Work registry、Completion／Fenceの確保は別に残る。Buffer資源のVulkan device memoryとも区別する。

## 構築・rollbackと所有

Poolはgraphics familyとDeviceを検証し、個別Buffer resetを許可するflagsで生成する。transientは任意の生成hintである。Bufferは正のcountとlive Poolを検証し、全CPU配列容量を確保してからnative割当を呼ぶ。native割当失敗はVulkanが割当全体を回収する契約に従い、未公開配列を空に戻す。

arenaはPoolを先に、Pool ID／直接owning Poolを参照するBufferを後に構築する。成功したslotだけを記録し、後続失敗時はBuffer→Poolの逆順でCPU実体とnative資源を回収する。外部Pool／Bufferは元のowner・Implを保存し、破棄しない。同じArenaMemoryは失敗後に再使用できる。

Pass等と同様、get_pool()は元Poolの正規control blockへ戻る。importer rootのaliasへ包み直さない。get_device()もDeviceの元ownerを返し、不要な子Bufferを延命しない。Deviceのshared_from_thisは順序9で除去する移行adapterである。

## Domainの明示接続

arena.command_buffer(id)は明示attach入口である。同じImpl・同じ正規control blockのlive Domainがあれば共有する。同じarenaの別slotはcontrol blockが同じでも別Domainとなる。Domain用の確保が全部成功してからweak参照とRecordingContextの借用を公開する。

最後のowning facadeを失うとDomainはdrainする。closing中に保持資源のcallbackを全部終え、それから自分のcontextだけを切断する。終了callback中の明示attachは拒否する。arena／importがnative実体だけを保持していれば、完全終了後に明示attachで新Domainを接続できる。

旧shared_ptr<Impl> constructorは引き続きResolveだけを行い、失効Domainを再生成しない。private Factoryの失敗・再入でも未公開Domainが既存contextを切断しない。Root内部へowning facade／Domainを保存しないため、Workが自分のarenaを保持しても循環しない。

## 公開操作と互換性

```cpp
vsl::CommandPool pool(device, {.transient = true});
vsl::CommandBuffer buffers(pool, 3);
auto manager = vsl::CommandManager::FromBuffer(buffers);
```

```cpp
namespace t = vsl::typed;
namespace l = vsl::lifetime;
namespace s = vsl::storage;
constexpr std::array<t::ResourceRequest, 3> requests{{
    {{1}, s::ResourceKind::COMMAND_BUFFER},
    {{2}, s::ResourceKind::COMMAND_BUFFER},
    {{3}, s::ResourceKind::COMMAND_POOL}}};
constexpr t::ResourcePlan<3> plan(requests);
const std::array<l::ResourceInit, 3> inputs{
    l::CommandBufferInit{t::ResourceId{3}, 2},
    l::CommandBufferInit{t::ResourceId{3}, 3}, vsl::CommandPoolCreateInfo{}};
l::ResourceArena arena(device, plan, inputs);
auto manager = vsl::CommandManager::FromBuffer(arena.command_buffer({1}));
```

既存constructor、DEFAULT_BUFFER_SIZE、コピー、independent、Phaseの送信／取消を維持する。FromBufferはnative ringを再割当せず、新しいManager facadeを作る。independentは以前と同じgraphics／present queueを引き継ぐ。

公開resetは記録／返却／変更処理中とGPU使用中の対象を拒否する。全resetは全対象を先に検査し、後方の未完了slotを見てから先行slotだけresetすることはない。未知の完了観測エラーでもnative resetへ進まない。SUBMISSION保持はresetでは解除せず、次の成功submitまたはDomain終了まで維持する。

nextは記録／返却／変更処理中を拒否するが、送信済みslotへのhost巡回は許可する。次Phaseのprepareがそのslotの前回送信を待つ。reset／nextは世代を更新し、prepare callbackから一周して元slotへ戻っても外側prepareが変化を検出する。resetのpoll callback中は新Phase／reset／nextの再入を拒否する。自身のfacadeがcallbackで破棄されても局所Domainが処理を保持する。

記録中のpublic resetに依存する利用側はphase.cancel()へ移行する。GPU再使用のために毎回resetする必要はなく、通常はPhase.prepareが待機とresetを行う。private Implのdevice／command_poolがshared_ptrだと仮定する利用側はowning getterを保存し、内部のraw参照の借用期間を守る。公開data_の廃止は順序12である。

## 検証と残る範囲

CPU2件・GPU12件・public compile1件を追加した。同型slotと隣接context、同rootの独立した2ring、Typed copy、自己rootを保持しても循環しない完了／取消、後続slot失敗と同領域再使用、Domain確保失敗時の未publish、終了callback中の再接続拒否と終了後再接続、importの元ownerとDomain非所有、Pool固定辺とgetter、別control blockの拒否を確認した。

記録中mutationの拒否、resetでのSUBMISSION保持、全slot事前検査、poll callbackの再入拒否とfacade自己破棄、元slotへ一周して戻る変更の検出も検証した。resetの未完了判定は未送信のunsignaled fenceで固定したsynthetic Workで検査し、実際のslot0 copyを後でsubmitして部分resetがなかったことを確認する。実GPUを任意の時間止めた検証とは区別する。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL904件中901成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約195件が成功した。変更C++26ファイル／18TUのstrict違反0、最後のテスト入力修正1TUも再検査成功。CMake314パスとMSBuild／filtersの新5パスの一意登録を確認した。

installed public consumer16targetのbuild／link／CPU実行、Debug／Releaseの実寸法assertとCPU利用、x86_64 cross archive、異artifactのlink拒否が成功した。heap／arenaサンプルの3frame・12submission・2draw・238831更新画素、resize1720×1080→2000×1360、pipeline生成1回を維持し、PNG3枚が一致した。VUID／sanitizer errorはない。

arenaではslotごとの個別Impl確保を除去した。内部vector、Domain／Work／結果／Fence、native driverとGPUメモリの確保は残る。性能率は未測定。実driver割当失敗／DeviceLost、Windows／Linux実機、LSan、CPU並行は未検証である。次はFence／Semaphoreの共通backend・配置と、PENDING予約からの明示reset／destroy拒否へ進む。
