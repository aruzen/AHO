# Swapchain世代に属する描画完了Semaphore

[完了計画](arena-completion-plan.md)の10dに入る前に、既定の描画完了Semaphoreを共通backendへ接続する。取得Fenceのpool化と取得後失敗の回収は後続であり、この単位だけで10d完了とはしない。

## 責務と配置

従来の既定経路は、DefaultPhaseがSwapchainImpl内の`vector<UniqueSemaphore>`を初回記録時に生成し、共通SemaphoreImplの使用登録を迂回していた。通常のSemaphoreと同じ構築・破棄・物理使用状態を利用する形へ変更する。

- storageのSwapchainLayoutは、実画像数Mを受け、Image群と任意のpresent Fence slotの後へSemaphore slotを一つ配置する。size／alignmentは実定義から生成したResourceSlotLayoutを使い、加算・整列のoverflowを検査する。
- operationは既存のSemaphore生成・逆順破棄を共用する。失敗したnative出力を採用せず、成功分だけを巻き戻す。
- lifetimeの世代ownerが、共通construct_semaphoreへslotとMを渡す。部分構築に失敗すると、成功済みSemaphore／Fence／Imageを回収し、CPU領域のclaimを返す。未採用のnative SwapchainはPendingが回収する。
- syncは共通SemaphoreImplのgraphics使用とWSI markerを扱う。世代側contextはImplへの借用pointerだけを持ち、arena自身を強所有しない。

CPU領域へ置くのはSemaphoreImplと隣接owner contextであり、M個のnative handleと使用一覧を入れる可変vectorの領域は別確保である。Vulkan内部の確保もCPU arenaに含めない。SwapchainImplのUniqueSemaphore vectorは除去する。

## viewと記録

`SwapchainAccessor::finished_semaphores()`は、世代の正規ownerとcontrol blockを共有するSemaphoreHolderを返す。元wrapper破棄後も使用でき、旧世代のviewを新世代へ付け替えない。世代内のImage／present Fenceともownerの重複排除を共有する。

```cpp
vsl::lifetime::SwapchainArena generation(device, surface, create_info);
auto chain = generation.view();
auto finished = chain.finished_semaphores();
// countは実画像数M。frames-in-flightのFとは独立。
auto phase = commands.start_phase<vsl::DefaultPhase>(chain);
```

DefaultPhaseはfinished省略時に、prepareの前に正規viewを選択する。prepareが解決したImplをsubmit／cancelでそのまま使い、途中で公開fieldが変わっても取消は元sourceを引き継ぐ。通常submitではsourceの差し替えを拒否する。signal要素は取得したimage indexで選び、FがM以下である条件を追加しない。

present側の省略入口も同じsourceとWSI markerへ正規化する。操作のたびに配置表や所有graphを探索する構造にはしない。

## 同期sourceの保持期間

正規finishedが世代ownerを持つため、従来のWork.resourcesへsourceを保存すると、graphics／presentation完了後にも送信履歴が旧世代を保持してしまう。同期source３役（wait／signal／external Fence）の所有はcompletion_resourcesへ統一し、Impl pointerとowner indexの借用を分ける。

終端保存後、lifetimeがraw source／indexとexternal native Fenceの借用を無効化してからowner一覧を解放する。明示的なSUBMISSION保持の資源と、内部owned Fenceの再利用は維持する。取消は元sourceを返却Workへ移した後に元結果を通知する。

Presentation／PresentRecordはWSI完了に必要なownerを別に保持する。graphics完了だけでpresent待機中のSemaphoreを解放せず、strict present proofまたは既存legacy契約の回収まで残す。保存Completionは結果だけを残し、送信slotの置換を待たずに不要な世代を回収できる。

## AHOの移行

Window生成とresizeは、候補Swapchainのfinished_semaphores()を使う。同Mでも旧finishedを流用せず、候補のFrameBuffer等とまとめて公開する。

`SynchroManager.get_semaphore(window_name + "RenderFinished")`への強い名前登録は終了する。長寿命のEngine registryが旧世代、Surface、OS windowを保持し続けるためである。現在の移行入口は`window.data2->render_finished`または`window.data2->swapchain.finished_semaphores()`である。F個のImageAvailable／InFlightの既存名前登録はこの単位では変更しない。公開data_等の廃止は順序12で扱う。

明示的な外部finishedを指定しても、世代の既定M個は先に生成される。未使用native Semaphoreのコストは残るが、初回記録時の生成・別状態管理へ戻さない。必要性と費用は未測定であり、性能向上率は主張しない。

## 検証と未完了

同owner／slotの整列と範囲、F>Mでのimage要素選択、Semaphore途中失敗のrollbackとCPU領域再利用、同M resizeでの新世代交換、終端後もWorkが残る場合の同期source回収を追加検証した。Mが1の場合も最初のSemaphore生成失敗を注入し、不要なskipを加えない。

sourceをWork寿命まで残す修正前の経路では、新しいCompute終端回収テストと既存の取得世代取消テストの2件が失敗した。修正後は両方を含む全VSL1,067件が成功し、既知のpacked format3件だけskipした。AHO GPU160件／CPU1,070件、compile契約195件、変更C++18ファイル／12TUのstrict検査、source370パスの検査も成功した。VUID／ASan／UBSanのエラーはない。

installed public consumer26targetのbuild／link／CPU実行、新getterの実呼出しと空source拒否、Debug／Releaseの寸法一致、x86_64 cross archive、混在artifactのlink拒否、temporary borrowの負契約を確認した。新しいprivate実装をinstallしない。

macOS arm64／Clang20／C++26／MoltenVK1.4.2で、heap／arena両サンプルのPNG3枚は一致した。3frame・12submission・2draw・238831更新画素、1720×1080→2000×1360のresize、pipeline生成1回を維持する。SwapchainImpl／slotは168／168 byteから144／144 byte（alignment8）になったが、別途追加したSemaphore slot128 byteやRoot contextを含む全体の削減量ではない。全体の確保数・時間・性能率は未測定である。

Windows／Linux実機、実DeviceLost、LSan、CPU並行、全native失敗位置は未検証である。描画完了Semaphore以外の取得後回復もこの単位の完了条件には含めない。

次はF個のacquire Fence、CPU-only proof、外部AcquireTicketによる取得後失敗の回収である。acquire signalとfinished signalの一度の消費、画像返却、native呼出中のPhase破棄、再生成が必要な世代状態は後続へ残る。
