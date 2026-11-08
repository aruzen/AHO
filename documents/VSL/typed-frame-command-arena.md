# Typed frameの実行資源とarena

順序8c。`TypedFrameCommands`で、command pool・ring・in-flight Fenceを同じbackendのheap／arenaから生成し、既存の`TypedFrameResources`／bundleへ接続する。順序11の既定変更前なので、新規生成のpolicyは明示する。

## 配置と寿命群

slot数Fに対して、Pool Impl×1、ring Impl×1（native command buffer×F）、Fence Impl×1（native Fence×F）を配置する。arenaは`ResourcePlan`の3slotを使う。可変配列のpayload、command Domain／Manager／Work／Result／予約tokenはCPU一括領域の外に残る。heapも同じoperation／lifetime backendを呼ぶ。

実行用ownerはアプリのper-frameデータarenaと分ける。Facadeは外側にあり、Manager・元ring・Fenceを保持する。arena rootや各Frame値へDomainを強所有させない。`TypedFrameResources`がWorkへ渡すのはFrame値だけなので、Frameがcommand実行群を所有しなければDomain→Work→Frame→Domainの循環を作らない。

元ringの追加保存は、公開`data_`の差替え互換を持つ現在のManagerに対する固定・検査用である。既存のFrameResourceDomainを共用し、取得ごとにManager／Domainを作り直さない。これは順序12で旧公開入口を閉じた後に簡素化を検討する移行用の保存で、native操作の参照経路へ挟まない。

`HeapStorage`／`ArenaStorage`の宣言は`storage/typed_storage_policy.hpp`へ移した。名前・namespace・型は維持し、タグの利用にdescriptor schema全体を必要としない。BindingMapはタグ側で前方宣言し、完全定義は従来どおりTyped配置計画側に置く。

## APIと利用

```cpp
namespace t = vsl::typed;
t::TypedFrameCommands execution(device, 3, t::ArenaStorage<>{});
// heap選択は t::TypedFrameCommands(device, 3, t::HeapStorage{})。
t::TypedFrameResources<MyFrame> frames(execution.manager(), make_frame);

auto phase = execution.start_compute();
frames.update(phase, update_frame);
const auto& frame = frames.get(phase);
phase << frame.input.copy_to(frame.output);
phase.submit();
execution.next();
```

`manager()`は同じManagerImplを共有するコピーを返す。`FromBuffer()`による別Managerの再生成はFrameResourcesの同一性を失うため行わない。`start_compute()`は常に外部Fenceを渡し、Phase内のheap UniqueFence生成を回避する。Phaseは移動不可なのでprvalueを直接返す。

既存ManagerとFenceを借り受ける所有入口もある。

```cpp
t::TypedFrameCommands execution(existing_manager, existing_fences);
```

取り込みは、Managerの所有、正規ring／Pool／Fence、同一Device、uint32 slot数とindex、F個以上のnative Fenceを検査する。構築時にwait／reset／予約を行わず、PENDING／使用中検査はPhase開始へ委譲する。空のshared ownerを持つManagerはdereference前に拒否する。旧`shared_ptr<CommandManagerImpl>`入口で任意の別control blockを強所有に偽装する入力は、元Impl所有をVSL側で証明できない。従来の信頼条件として残し、順序12でその公開入口ごと除去する。Native ring／Pool／Fenceはowner nodeで正規control blockを検査する。

開始・取得・巡回では元ringのpointer／control block／count／indexと元Poolを照合し、公開fieldの差替えによって別ringへ切り替わる互換fallbackを使わない。移動代入は組全体をswapしてから旧Domainを解放し、旧callbackが新しいManagerとFenceの組を観測できる。

## 再利用・完了・取消

Computeの巡回は`next()`で明示する。記録中の巡回は既存の検査で拒否する。次のPhaseは対象slotの前回使用完了を待って同じFenceを予約する。明示resetは追加しない。更新失敗／取消後にも同じslotを再利用できる。

FacadeやFrameResourcesを破棄しても、作成済みPhaseはManager／Domainと実体ownerを保持する。COMPLETION保持のFrame値はGPU完了／取消で回収し、結果だけの保存は実体を延命しない。成功送信のSUBMISSION資源は、slot置換／Domain終了まで保持する。送信されなかったWorkはPhase等の最後の保持が外れる時点で破棄される。

## 検証と後続

両policyで3slotを12回使い、元のFrame値とnative Fenceの再利用、`owned_fence`未生成、取消・更新失敗後の再利用、元facade破棄後の完了／回収、同一Managerと別Managerの区別、移動代入のcallback順、非所有Manager・不足Fence・別Device・偽Fence owner・ring／Pool差替え拒否を確認した。既存uniform／descriptorのGPUテストも両policyの実行facadeへ接続し、12 dispatchと各slotのreadbackを行った。arena版bundleの配列binding／shader／readbackも新しい実行facadeと3frame巡回へ移行した。

追加15件（CPU1／GPU14）、重点22件、VSL941成功・既知packed-format3skip、AHO GPU150／CPU1070、compile195成功。全回帰後に追加した最後のbundleテスト接続差分は、該当GPU1件と規約検査で確認した。validation／ASan／UBSanエラーなし。strictは全6ファイル／4TUと最終変更の2TUを個別確認し、すべて違反0。CMake324path、installed public consumer19targetが成功した。heap／arenaの描画・resizeはPNG3枚が一致し、3frame／12submission／2draw、Pipeline生成1回を維持した。

Impl寸法と生成fingerprintは親の同期資源配置から不変。構成別生成／x86_64 cross archive／異なるartifact拒否はその確認済み証跡を継承し、今回の追加公開APIはinstalled consumerで検証した。実driverの失敗注入・DeviceLost・Windows／Linux実機・LSan・CPU並行利用・性能率は未検証である。

AHOのEngine／Windowの生成は起動arenaとWSIを扱う順序9／10へ残す。image-availableはF、render-finishedはswapchain画像数Mに属し、同じframe ownerへまとめない。DefaultPhaseが生成するacquire Fenceもgraphics Fenceとは別のWSI予約・取消返却として順序10で扱う。順序8cだけで厳密presentation完了や起動資源配置を完了とはしない。内部可変確保・所有操作の最終測定と既定policy変更は順序11／12で行う。
