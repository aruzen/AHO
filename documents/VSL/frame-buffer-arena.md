# Framebufferの共通backendとCPU配置

[完了計画](arena-completion-plan.md)の7c。通常のFrameBufferとResourceArenaが、同じ生成・検証・記録・破棄処理を利用する。一つのImplが複数native framebufferとattachment行列を持つ既存の意味を維持する。

## 責務と配置

operationは全行の画像のdevice／format／sample／extent／layers／usageを検証し、native framebufferを生成・破棄する。clear値とareaの検証・native変換、begin／end／next-subpassのVulkan呼出しもoperationへ置く。lifetimeは指定領域へのCPU実体構築、所有、構築失敗の回収とGPU使用後の破棄時点を管理する。

ImplはDeviceへの借用と保存済native Device、native framebuffer配列、Passへの借用、Imageへの借用行列、extent／layers、owner nodeとlifetime contextへの借用を持つ。Device／Pass／Image／arenaを強所有しない。独立したGPU UseStateも作らず、Imageの共通物理UseStateを利用する。

storageのslotはImpl用aligned byte配列と、getter用lifetime contextのaligned byte配列を隣接して持つ。Contextもbyte領域から明示構築し、slotのstandard-layout性とImpl先頭offset 0を検査する。通常操作はImpl内のraw参照へ直接届き、contextを使うのはPass ownerの取得時だけである。contextの個別確保やcontrol blockは追加しない。

VSLは`FRAME_BUFFER_IMPL_SIZE/ALIGNMENT`と`FRAME_BUFFER_SLOT_SIZE/ALIGNMENT`を実定義から生成する。ImplLayoutは本体、ResourceSlotLayoutはcontext込みslotを表す。Typedの配置計算には後者を使う。USE_OFFSETはNO_SEPARATE_USEであり、このcontextをGPU使用状態と混同しない。private定義とのactual static_assert、compile-only probe、構成別header、artifact guardとinstallへ接続する。

CPU一括領域に含むのはImpl本体とcontextである。Impl内部のvector、生成scratch、arena管理metadata、Vulkanのdriver内部確保とImageのGPUメモリは別に残る。

## 構築・失敗と所有

FrameBufferInitはPass source、width／height／layers、attachment行列を持つ。行がnative framebuffer番号、列がAttachmentIdである。同じImageを複数行へ指定できる。IDまたはowning accessorから解決する。

Passは記録・clear検証にも必要な永続依存であり、[Pipeline](pipeline-arena.md)の生成時だけのPass sourceと区別する。直接のPass／Imageはrootの固定親へ保持する。同じarena内では借用に正規化する。外部sourceのcontrol blockとprovenanceを保ち、既知root間の逆向きdescriptor依存を拒否する。

全行の形とCPU確保をnative生成前に終える。native handle配列を先に確保し、生成成功後は対応要素へ代入する。後続生成で失敗した場合は成功分を逆順に破棄し、handleを空に戻す。lifetimeはImpl→context→slotを破棄する。ArenaではImage／Passより後に構築し、成功した実体の逆順で巻き戻す。外部sourceは破棄しない。

heap ownerも同じslot／backendを利用する。rootが所有するPass／Image／swapchain依存はvisitorへ列挙する。Impl内へrootの強い所有を作らない。

## APIと正規owner

通常の単一target、FromAttachments、Swapchain／IDPicking constructorを維持する。swapchain constructorは画像を行へ展開するadapterで、必要なswapchain ownerを外側rootへ保持する。swapchain本体の配置・presentation終了は順序10である。

```cpp
vsl::FrameBuffer normal(pass, width, height, {color});

namespace t = vsl::typed;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 2> requests{{
    {{10}, vsl::storage::ResourceKind::FRAME_BUFFER},
    {{20}, vsl::storage::ResourceKind::FRAME_BUFFER}}};
constexpr t::ResourcePlan<2> plan(requests);
const std::array<l::ResourceInit, 2> inputs{
    l::FrameBufferInit{pass, width, height, 1, {{first_image}, {second_image}}},
    l::FrameBufferInit{pass, width, height, 1, {{first_image}}}};
l::ResourceArena arena(device, plan, inputs);
auto target = arena.frame_buffer({10});
auto original_pass = target.get_render_pass();
```

`frame_count()`は行数、`owner_ref()`は正規ownerを返す。get_device()は実Deviceのownerを返し、Framebuffer rootを延命するaliasを返さない。get_render_pass()はcontext内のWeakOwnedRefから元のPass ownerを取得する。外部Passだけを残しても不要なFramebuffer／Imageを保持しない。同rootのPass weakは、全実体とnodeをpublishした後に設定する。

外部Framebuffer importのgetterも元のFramebuffer ownerを返し、importerへ包み直さない。公開data_／shared_ptr入口とDeviceのshared_from_thisは順序9／12の移行対象として残る。private Implのdevice／render_pass／attachmentsを読んでいた利用側は借用pointerへ移行し、保持にはowning accessorを保存する。

## 記録・完了・取消と再入

RenderPassBeginは、poll前にFramebuffer owner、対象index、CB slot／native handle、area／native clear scratch、Completionを局所保存する。WorkはFramebuffer rootをSUBMISSION方針で一度保持し、各Imageの共通UseStateへ使用を登録する。Pass／Imageのownerを重複してpinしない。

完了観測中のcallbackがBegin自身や元wrapperを破棄しても、以後はsnapshotだけを使う。Image登録後に同じ記録結果・slot／native CBを検査し、取消や別の記録へ切り替わった場合はnative beginを実行しない。同じCBへのBegin／End／NextSubpassの再入はhost guardで拒否する。guardは例外時にも解除する。

clear／areaはconstructorと同じoperation処理でinvoke時にも検証する。legacy公開fieldの不正な編集をnative記録前に拒否し、同じPhaseで正しい入力を使い直せる。高位のRenderPassBegin記録にはPENDINGな記録Phaseが必要である。Phase外の手動invokeへ依存していた場合は、以下の形へ移行する。

```cpp
auto phase = manager.start_phase<vsl::ComputePhase>();
phase << vsl::command::RenderPassBegin(target, {}, {}, 1)
      << pipeline << viewport << scissor << draw
      << vsl::command::RenderPassEnd{};
phase.submit();
auto completion = phase.completion();
completion.wait();
```

Completionは結果だけを所有する。GPU完了後のSUBMISSION保持はCB slotのWork置換まで続く。cancel時は記録をresetし、PhaseのWork所有がなくなった後に回収する。順序8でこのWork registryをNative Implの外へ移す。

## 世代更新と範囲

新しいFramebuffer／Image世代を別rootで構築してから公開する。構築失敗時に現在のCPU facadeを維持でき、保存Begin／viewは旧世代へ届く。互換Passとdynamic viewport／scissorのTyped Pipelineを新旧targetで再利用でき、Shared Pipeline／Layoutが旧attachment世代を延命しない。

これはCPU側のFramebuffer／所有世代の保証である。新swapchain生成後にold swapchainがretiredになるWSIの性質は別であり、失敗後の旧swapchain acquire再開まで保証しない。復旧・presentation完了は順序10で扱う。

private NativeFactoryを使い、生成の2回目／4回目に例外を注入して先行native handleの逆順破棄と再試行を検証する。factoryは構築呼出し中だけ使い、Impl／記録へ保存しない。実driverのvkCreateFramebufferがOOM等を返す状況を強制した検証とは区別する。

## 検証と残る範囲

CPU2件・GPU13件・public compile1件を追加した。同型slotの直接参照、contextの配置とalignment、複数行と複数attachment、固定依存の循環拒否、importの正規owner、途中失敗・同領域再利用、保存command／Typed描画、完了／取消後の回収、poll callbackによる自己破棄・取消・再入、legacy可変fieldの再検証、世代更新と互換Pipeline再利用を確認した。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL880件中877成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約194件が成功した。変更C++22ファイル／15TUのstrict違反0、CMake307パスとMSBuild／filtersの新4パスの一意登録を確認した。

installed public consumer15targetのbuild／link／CPU実行、Debug／Releaseの実寸法assertとCPU利用、x86_64 cross archive、異artifactのlink拒否が成功した。private backendはinstallしない。このarm64構成ではImpl104 byte、context込みslot128 byte、alignmentはいずれも8で、11種を一つずつ置く計画は1040 byte・alignment 8となる。

heap／arenaサンプルはいずれも3frame・12submission・2draw、238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回を維持し、PNG3枚が一致した。VUID／sanitizer errorはない。実driverのOOM／DeviceLost、Windows／Linux実機、LSan、CPU並行と性能率は未検証である。

順序8ではWork registryをnative CommandBufferImplの外へ移し、command pool／buffer／fence／semaphoreの配置へ進む。現在のDevice shared_from_this、公開data_／shared_ptr入口、Impl内部vectorとnative driver／GPUメモリの個別確保は別の移行・監査対象として残る。
