# OS window／Surfaceの所有分離とCPU配置

順序9c。Instance／PhysicalDeviceに続き、Surfaceを通常heapと指定領域で共通のbackendへ接続する。Device側とWindow／Surface側の寿命群は分ける。公開StartupArenaとDeviceの配置は9d、swapchain／presentation完了の整理は順序10で行う。

## 責務と依存

| 層 | この変更の担当 |
| --- | --- |
| storage | 生成済みのSurface Impl／slot寸法・alignment。領域と配置先は呼出し側が決める |
| operation | GLFWの初期化／終了・native windowの生成／非表示／破棄、Vulkan Surfaceの生成／破棄 |
| lifetime | GLFW lease、独立OS owner、Surface構築／rollback、固定親と弱いgetter context、main thread退役queue |
| sync | 従来のGPU完了観測を維持。Surfaceにgraphics fence相当の使用状態を追加しない |

SurfaceImplはInstance／OSWindowImplへの解決済み参照とnative handleを持つ。通常操作で所有graphを探索しない。Surfaceの外側ownerが元InstanceとOS ownerを固定保持し、getter contextは元親へのweak参照だけを持つ。同一arena内の子からarenaへの強参照は作らない。

OSWindowOwnerはnative windowとGLFW leaseだけを所有する。WindowData／plugins／Engine／描画状態は所有しない。したがってWindowData→Surface plugin→OS ownerからWindowDataへ戻る循環はない。Device生成時のSurfaceは一時的なqueue選択入力のままで、Deviceの永久親へ追加しない。

## 論理closeとnative破棄

`PureWindow::close()`はlogical handleを無効化し、WINDOWS登録とplugin／updateを切り離してnative windowを非表示にする。Surface、Swapchain、入力の所有者が残ればnative window自体は存続する。閉じたAHO Windowのnative操作／照会は、保持中のOS windowがあっても拒否する。

最後のOS owner／leaseのreleaseは、どのthreadでも退役登録だけを行う。intrusive queueのnodeは実体に含め、退役登録用の追加確保はしない。native window破棄とGLFW終了はmain threadの安全な境界へ延期する。

- `GLFWCallScope`はGLFWを含む操作全体を囲み、最外scopeの前後に退役を回収する。
- `GLFWCallbackScope`はnative callback中の回収を禁止する。callbackのデストラクタでは回収せず、GLFWの呼出し元へ戻ってから行う。
- `GLFWLease::Drain()`はmain threadかつnative操作／callbackの外でだけ呼ぶ。生存中のownerを強制解放しない。
- noexceptの入力callback登録解除にはnothrow Scopeを使い、回収は外側scope／明示Drainへ任せる。main threadの前提を緩和する機能ではない。

最後のreleaseがworker上で発生し、その後event pumpを実行しない場合、host側がmain threadで明示Drainを行う。プロセス寿命のqueue metadataは静的破棄を行わず、native cleanupはこの境界に限定する。別TUのstatic ownerが破棄済みmutexへ触れる順序依存を避ける。

古いruntimeはweak ownerが失効してもnative終了まで登録を残し、同時に新runtimeを初期化しない。構築前のcontrol block確保失敗で未登録実体だけがqueueへ入っても、退役処理は自分と一致するepochだけを解除する。

外部のraw GLFW呼出しを混在させる場合、managed leaseが存続する間に`glfwTerminate()`を呼ばない。外部callbackにもCallbackScopeを置き、GLFWから復帰した安全な境界でDrainする。非Apple環境では最初のmanaged呼出しをhostのmain threadから行う。

## 共通backendと配置

通常の`Surface(Window*, VulkanAccessor)`とprivate配置入口の`construct_surface(slot, instance, os_window)`は、同じ`operation::initialize_surface()`を使用する。成功したnative handleだけを採用し、失敗時はImpl／weak contextを逆順に破棄する。再試行時は同じslotを利用できる。

`SurfaceSlotStorage`はImplと隣接weak parent contextのaligned byte領域である。Impl先頭offsetは0。`ImplLayout<SurfaceImpl>`と`ResourceSlotLayout<SurfaceImpl>`のsize／alignmentを、実定義を使った対象compilerのCMake probeから生成し、library内のstatic_assertとartifact識別で整合性を検査する。公開ヘッダーとinstall先へ定数を配布し、private Impl／backendは公開しない。

Surfaceを含め寸法traitは18型、Device資源用ResourceKindは15種のまま。起動資源をDevice引数が必要なResourceArenaへ混ぜない。今回の指定領域APIはprivateのままで、公開StartupArenaへ接続する前段である。host環境の実寸法はImpl 56 byte／slot 104 byte／alignment 8 byteだが、利用側は生成定数を用いる。

通常の入口は既存のplugin生成を使う。`instance`はwindow用extensionを有効にした正規VulkanAccessorとする。

```cpp
vsl::PureWindow window("sample", 640, 480);
auto surface = window.add_plugin<vsl::Surface>(instance);
auto os = surface->get_window_owner();
window.close();                    // 登録とpluginを切り離す。
// surface／osが残る間、元native windowは有効。
surface.reset();
os = {};
vsl::lifetime::GLFWLease::Drain();  // main threadのnative呼出し外で回収する。
```

同じImplを別offsetへ置く試験では、Instanceを共有する別々のOS windowを使う。1 native windowに複数のlive Surfaceを作る前提は置かない。slotの整列・容量・構築済み判定・成功済みslotだけの逆順破棄は配置owner側の契約とする。

## AHOと互換入口

AHO Windowはwait／native呼出しの前にData、描画状態、Engine／Manager、OS ownerを保持する。戻り後はcloseや対象の差替えを検査し、閉じたWindowへ新しいswapchainを公開しない。closeは論理的な閉状態を先に公開し、完了callbackからの再closeで二重waitしない。

`InputManager(Window*)`から作る組込み入力はOS ownerを共有し、ManagerやWindowより長く存続できる。Keyboard／Wheelのコピーも同じownerを引き継ぎ、native callbackの登録解除まで保持する。入力ごとの新しいowner control blockは増やさない。`InputManager(void*)`と偽handleを使う既存CPU backendは従来の借用契約を維持する。

互換の`.data_`、SurfaceのsharedImpl構築入口、SwapchainのSurface shared所有は後続移行adapterであり、順序12までに整理する。純粋なhost plugin登録はnative windowを必須にしない。AHO facade自身を同期callbackで任意にdeleteする一般保証は追加しない。close／共有Dataの変更に対する再入を扱う。

## 検証範囲と残作業

native callback中のcloseとpoll拒否、worker退役とmain回収、runtime再起動、未登録実体の退役後の新epoch保持、同型2slotと元親の所有、native失敗出力の未採用、後半失敗のrollback、DeviceとSurfaceの独立回収を対象にする。Surface／Swapchainを残したlogical close後のnative照会・再生成と、AHO入力コピーの寿命も確認する。

追加19件（VSL15／AHO4）、重点VSL88／AHO37が成功した。MoltenVK／Apple M1でVSL997成功・既知packed-format3skip、AHO GPU154／CPU1070、compile契約195成功。validation／ASan／UBSanエラーはない。strictは37file／22TU、GPU専用header補足3file／2TU、最終test変更各1TUを確認して実違反0。CMake344pathとMSBuild／filtersの新10pathの一意な登録も確認した。

installed consumer22target、Debug／Releaseの生成・実行、x86_64のcompile-only probe／archive、異なる構成header／binaryのlink拒否が成功した。一時OSWindowOwnerからnative handleを借用する入口もコンパイルで拒否する。既存15資源の1416 byte計画は不変。heap／arenaの描画・resizeは3frame／12submission／2draw、1720×1080→2000×1360、Pipeline生成1回を維持し、PNG3枚が一致した。独立レビュー3名を反映した。

実driverのOOM／DeviceLost、Windows／Linux実機、LSan、CPUの並行操作と性能率は未検証。worker上の最後のreleaseとmain回収は検証したが、native操作全体を任意threadから呼べる保証ではない。

graphics完了とpresentation完了の厳密な区別、retired swapchain生成失敗からの復旧、AHO起動群と実行群のarena化は未完了。既存入力のmulti-window callback登録方式は今回変更しない。GLFW／OS／Vulkan内部、shared owner境界、Impl内部の可変長containerの確保はCPU slotとは別に残る。性能改善率は未測定である。

検証中、`resize(-1, 0)`をGLFW error callbackの発生条件にはできず、この環境では0 extentのswapchain生成まで到達することを確認した。native callback試験は登録済みcallbackを直接呼んで行う。無効なresize引数のnative前検査と、最小化等で0 framebufferとなる場合の再生成延期は順序10で扱い、今回の9cで修正済みとはしない。
