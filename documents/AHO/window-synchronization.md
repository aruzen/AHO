# Windowのframe同期群とpresent方針

4責務／arena移行の10e1。Windowのframe同期とSwapchain世代を別の寿命で保持し、Engineへ指定したpresent方針を実際のWindow生成へ伝達する。

## 配置と寿命

Window生成時のcommand ring数をF、Swapchainの実画像数をMとする。WindowのResourceArenaへ、available SemaphoreImpl、graphics FenceImpl、acquire FenceImplの3slotを配置する。それぞれのnative配列はF個で、graphics Fenceだけ初期signaledとする。WindowDataは3つの正規aliasを持ち、同じarena内の参照を別ownerへ包み直さない。

finishedは既存のSwapchain世代ownerに属し、M個のnative Semaphoreを持つ。同Mの再生成でも新しい世代のsourceへ変わる。WindowのF群はresizeで作り直さない。CPU Implの3slotと、Impl内vector／native配列／VulkanのGPUメモリ確保は別である。

DrawPhaseはWindowのF acquire sourceをDefaultPhaseへ渡す。通常経路ではCommandDomainの遅延acquire arenaを作らない。Work／AcquireTicketは必要な所有境界で元rootを保持し、同rootのsourceは既存の保持処理で重複排除する。生成・操作・同期・破棄のVSL backendは共通のまま。

F群はEngineのSynchroManager名mapへ登録しない。WindowDataと外部holderの所有がなくなれば、実使用中のWork／Ticket等が必要な寿命だけ保持する。DeviceやEngineが生存しているだけでWindow同期群を残さない。保持中RenderContextも含めたclose時の共有状態解放は10eの終了処理で続ける。稼働中にcommand ringのFを変更する場合は、対応するWindow同期群の再構築も必要である。

## 明示したpresent方針の伝達

`EngineConfig.present_fences`をInstance／Deviceの能力要求と、EngineData内のWindow既定値へ保存する。初回Windowと追加Windowが、その値をSwapchainCreateInfoへ渡す。supported／enabled featureからpolicyを推測しない。

```cpp
aho::engine::StandardEngineConfig config;
config.engine.present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE;
aho::StandardEngine engine(config);

aho::Window same_policy("strict", 640, 480, engine);
aho::Window legacy("legacy", 640, 480, engine,
                   vsl::PresentFencePolicy::LEGACY);
```

Window constructor末尾のoptional policyで既定を上書きできる。REQUIRE_FENCEには、Device生成時からmaintenance1が有効である必要がある。Window生成はfeatureを後付けで有効化しない。resizeはそのWindowの現在のpolicyを保持する。

## 互換入口と移行

既存の4引数までのWindow呼出しは維持する。新しいWindowData fieldは末尾に置くため、旧positional aggregateの末尾standard_drawを保つ。手動構築したWindowDataでacquire sourceを省略した場合は、従来のCommandDomainによる遅延生成へ接続する。通常生成は配置済みsourceを使い、このfallbackは旧custom構築との互換入口である。

Engineの名前mapからWindow内部の生成名を検索する利用側は、Windowが持つholderへ移行する。

```cpp
auto available = window.data2->image_available;
auto completion = window.data2->in_flight;
auto acquisition = window.data2->acquire_fences;
```

EngineData／WindowData全体を構造化束縛しているコードは、field追加に合わせた変更が必要である。サンプルは必要なメンバーへの直接参照へ移行した。公開data_入口の廃止と最終getterは順序12で扱う。AHOの公開構造体とconstructorのABI変更を含むため、prebuilt利用側は再buildする。

DrawPhaseの構築は一度取得したWindowから、Swapchain・F/M同期・RenderPass／Framebufferを参照する。以降のcontextやresizeまで対象を固定する変更、0 framebuffer時の休止、closeの統合は10eの次の単位に残す。今回だけで10e全体の完了とはしない。

## 再生成候補と公開

10e2ではWindowDataのidentityを維持し、未公開のSwapchain候補を同じ共有状態へ保持する。Native生成へoldSwapchainを渡した後、旧世代は成功／失敗によらずRETIREDとなる。新Swapchainの構築が済んでからFramebuffer生成が失敗した場合、その候補はACTIVEのまま保持し、同じframebuffer extent／usage／present方針の再試行ではattachment構築だけを行う。

要求が変わった場合は、現役の候補を次のoldSwapchainとして置き換える。既に退役した世代はoldへ渡さない。候補のNative生成自体が失敗した場合は、共通VSL backendのSwapchainCreateProgressによりNative開始済みであることを確認して要求だけを残す。これはpreflightを通過したという意味であり、次回のNative生成成功や現在のSurfaceへの適合を保証しない。不正な追加usage等でNative前に拒否した要求は残さず、以前の検証済み要求や候補を保つ。

RenderPass／Framebuffer／M finishedを全部準備してから、公開4fieldと候補状態を交換する。旧facadeは交換後まで局所値で保持し、旧ownerの破棄から混合世代を観測させない。formatが同じ場合は元のRenderPassを保持し、動的viewport／scissorを使う既存Pipelineを再利用できる。F同期群は交換しない。

resize hookの一覧確保も公開前に行い、hookは公開済みの一組を参照する。hook自身が投げた例外は呼出し側へ伝播するが、完了した世代公開を巻き戻さない。候補保持に追加の世代wrapper／control blockは作らず、元のSwapchain ownerのaliasを保持する。

## 0 framebufferでの休止

10e3では論理Windowサイズと、実framebufferのpixel extentを区別する。`Window::resize`／`PureWindow::resize`の非正値はGLFWへ渡す前に拒否し、実framebufferが0の場合は再生成と取得を延期する。Windowの公開世代はACTIVEのまま保ち、追加usageがNONEでも延期要求を保存する。

延期するusageはSwapchain生成と共通のVSL検証へ通し、不正な追加要求で既知の延期要求を汚さない。復帰時のpreflight失敗でも元の延期要求を保持し、Native生成へ進んだ後または候補の採用時にだけ消費する。Surfaceの能力は復帰時にも再検査する。

DrawPhaseはWindowとcommand managerを一度snapshotし、基底の取得準備を延期してからsetupする。0なら`SUSPENDED`となり、slot・acquire・Fence reset・initialize／finishを進めない。休止したPhaseは復帰に再利用せず、新しいPhaseを作る。

```cpp
aho::DrawPhase phase(engine);
if (phase.recording())
    phase << commands;
```

休止中のcommand／delegate／tuple入力はinvoke前に拒否する。`target_window()`はPhaseに保持されたWindowのconst lvalue借用で、一時Phaseからの借用は削除している。context・標準draw・Screenshot・終了時の再生成も同じWindowを利用する。

VSLのDefaultPhaseは、Domain可用性の検査後、slotを進める前にprotected `acquisition_ready()`を呼ぶ。既定はtrueで、AHOが実Windowの状態を判定する。旧Work／graphics Fence観測後と、acquire Fenceの観測後・reset前でも再検査する。途中で0になった場合は未取得TicketをNOT_ACQUIRED、未送信WorkをCANCELLEDとして保存し、自分の準備状態が保たれている場合だけslotを戻す。休止状態の確定後に保持を解放し、peer Phaseのslotを変更しない。

Native reset自身のcallbackで初めて0になった場合、そのresetを取り消すことはできない。新しい未取得proofを解除して画像取得を止め、以前のCOMPLETE行は復元しない。同setupの再入と、0を使って同Domainの記録中Phaseを覆う入れ子も拒否する。

initialize／finishは実行中のclosureを局所値で保持する。公開`lifetime/phase_liveness.hpp`のstack guardにより、callbackでPhaseが破棄された後はthisへ触れない。Screenshotも前frame完了の観測後に現在の記録状態／Completion／targetを再検査し、cancel済みのPhaseへfinishを登録しない。ここでのstd::functionのcopyが確保するメモリは、今回のImpl配置領域には含まれない。

## WindowのcloseとSurface退役

10e4では共有WindowDataへ`closed`を持たせる。closeは先に退役要求を登録し、全コピーから新しい描画を拒否したうえで、世代・Surface・F/M同期・Framebuffer・RenderPass・描画cache・未公開候補を局所所有値へ移す。全fieldが空になってからplugin破棄・対象Phaseの取消・回収を行う。保存WindowやRenderContextはCPU上の閉じた共有状態だけを保持し、GPU資源を残さない。

通常の`Window::close()`とtitlebarのcloseは同じ処理を使う。AHOのlistenerはnative WindowData・描画状態・元Device／Surfaceを弱く参照し、自身でWindowやarenaを延命しない。描画状態だけ先に失効した場合も、従来のQuietCloseと同じnative closeを行う。低位の`PureWindow::close()`を明示的に直接呼ぶ利用は、AHOの描画取消を経由しない。

対象Phaseはnative WindowDataと描画状態の両方で識別する。取消要求を先に立て、callbackでPhaseが削除された場合もTLS先頭から再探索する。途中の取消が例外になっても残りを処理し、回収を試みた後に最初の例外を再送出する。plugin破棄中にPhase自身が削除されても、共有closed状態の検査によりユーザーfinishを呼ばない。

AHOの既定RenderPassBeginは`ResourceRetention::COMPLETION`を指定する。FBOをgraphics完了まで保持し、presentation中の世代は別のPresentRecordが保持する。通常VSLのRenderPassBeginは従来の`SUBMISSION`を既定値として残す。custom initializeで選択したSUBMISSIONや、外部に保存されたcommand／view自身の所有は、明示的な外部保持として残る。

Native処理中のcloseは論理closeだけを即時に完了し、同Deviceへの返却submit／idle／画像releaseを延期する。未送信WorkはNative resetを行わずCPU記録を取消し、元CompletionをCANCELLEDにする。既に送信済み／不明の結果やenqueue分類は書き換えない。Domainが消える場合も、未完了Workを元CB ownerごとquarantineへ移す。

閉じたSurfaceだけを登録するVSLのcold queueは、元Device／Surfaceのcontrol blockとslotを弱く保存する。同じarenaの異なるslotを混同せず、weak失効後にraw pointerへ触れない。callbackからの新規登録を別chainへ受け、例外後も未処理・pendingを復元する。DeviceLostという状態と、実際に未回収であることは区別する。

```cpp
// closeの登録に失敗した場合は、Windowを閉じる前に例外になる。
window.close();

// 管理外のループでも、元Deviceを使って保留分を明示的に回収できる。
const auto status = vsl::lifetime::collect_surface_retirements(
    device, vsl::lifetime::SurfaceRecoveryMode::WAIT);
```

通常のPhase setup／submit／presentとCompletion観測／waitの外側scopeでも、Native結果のcommit後に登録済みSurfaceだけをPOLLする。Native guardのdestructorでは回収しない。明示的なFenceHolder wait／CommandDomain wait中に登録された要求は、次の対応境界か明示collectで処理する。queueとGLFW操作は従来同様に同じ管理スレッドで使う。

CPU queue nodeはclose時の個別確保として残り、外側scopeのDevice保持にも参照カウント操作が残る。Impl配置領域やGPU device memoryとは別で、性能改善率は未測定である。

Windowの公開呼出し方と通常VSLの保持既定値は維持する。RenderPassBeginの末尾引数とWindowDataの共有状態を追加したため、prebuilt利用側も対応するヘッダーとバイナリで再buildする。

## 10e1の検証

新GPU7件／CPU1件と既存strict policyの拡張で、F群の同root／別Impl、Engine生存中の回収、同名Windowの独立、resizeでのF保持とM交換、初回／追加Window／overrideのpolicy、取得source省略の互換、Window一回取得、null／未起動Engine拒否を検証した。

macOS arm64／Clang20／C++26／MoltenVK1.4.2、ASan／UBSan／Vulkan validationで、重点GPU9件、AHO GPU全168件／CPU全1,071件、compile契約195件が成功した。heap／arenaのPNG3枚は一致し、3frame／12submission／2draw／238831更新画素、1720×1080→2000×1360のresize後もPipeline生成1回を維持した。

変更library10ファイルを初回6TUと修正後のCPU／GPU variant補完で検査し、規約違反0。サンプルMainはformatterと実buildで検証した。source path376件もエラー0。installed public consumer3種（既存2＋新Window API）がbuild／link／CPU実行に成功した。VSL backend／実寸法入力は変更していない。

policyを従来のLEGACY固定へ一時的に戻した比較では、初回・追加Window・resizeの3期待値で回帰1件が失敗した。byte一致で復元し、全target再build後に同1件が成功した。実driver故障、他OS実機、CPU並行、LSan、性能は未検証であり、改善率は断定しない。

## 10e2の検証

新VSL2件／AHO4件で、Native前後の進行情報、途中失敗、同候補retry、要求変更時の退役、不正追加usageの非保持、F群の不変と一括公開を確認した。既存の標準描画resize試験も、保持Context／cache→Framebuffer失敗→同候補retry→新extentでのreadbackへ拡張した。

同じGPU環境で重点7件、全VSL1,114件（既知skip3）、AHO GPU172件／CPU1,071件、compile195件が成功した。heap／arenaの同じPNG3枚とPipeline生成1回を維持した。strict13ファイル、source376、installed public consumer28種、Debug／Release／x86_64配置情報、混用ABIと借用負例も成功した。

Native開始通知だけを無効にした比較では新VSL2件が失敗し、byte一致で復元・全target再build後、重点7件が成功した。失敗注入は実Native生成と即時破棄の後に合成OOMを返す方法で、driver内部の実OOMを起こしたものではない。

## 10e3の検証

新VSL3件（GPU2／CPU1）、AHO10件と既存snapshot試験の拡張で、準備3境界での休止、実reset中の0化、NONE要求の復帰、Native前の無効resize拒否、全command入口の拒否、setup再入、通知／query中のclose、closure自身の破棄、Screenshot観測中のcancelを確認した。

macOS arm64／Clang20／MoltenVK1.4.2、validation／ASan／UBSanで重点14件、全VSL1,117件（既知skip3）、AHO GPU182件／CPU1,071件、compile195件が成功した。heap／arenaのPNG3枚一致とPipeline生成1回を維持した。strict26ファイル／19TU、source377、installed public consumer28種、Debug／Release／x86_64配置情報、ABI混用と借用負例も成功した。

0はprivate framebuffer query seamで観測させ、Native取得／resetの回数と旧Fenceのsignaled状態を実driver上で確認した。実OSの最小化イベント、他OS実機、CPU並行、LSan、性能改善率は今回の検証に含めない。titlebarを含むclose統合は10e4へ続く。

reset前gateだけを無効にした比較では、取得前の外側判定が有効なままでも、acquire Fence観測後に不要resetが1回起き、旧Fenceのsignaled状態が変わることを回帰1件の2期待値で検出した。byte一致で復元・全target再build後、重点14件と最終install consumer3種が成功した。

## 10e4の検証

新VSL7件／AHO8件と既存の取得中取消回帰で、同root別Surface slotの区別、weak登録・重複排除・再入追加・失敗後retry、SUBMISSION／COMPLETIONのFBO保持差、取消callbackが開始したpeerのslot保護を確認した。Windowコピー／保持Context／titlebar／描画状態失効／plugin破棄中Phase削除／facade自己破棄、Native reset・acquire・submit・present中closeも検証した。別SurfaceのNative submit／presentでは、対象Phaseの保持／その場での孤児化の4条件で、Native中の追加submit／idle／releaseが0であった。

同じmacOS arm64／Clang20／MoltenVK1.4.2、ASan／UBSan／validation環境で、重点16件、全VSL1,124件（既知skip3）、AHO GPU190件／CPU1,071件、compile195件が成功した。heap／arenaのPNG3枚一致、3frame／12submission／2draw／238831更新画素、1720×1080→2000×1360、Pipeline生成1回を維持した。

strictは初回30ファイル／19TUと最終Phase補完2ファイル／2TUを合わせて変更31 C++ファイル／20TUの違反0、source path378件もエラー0。installed public consumer28種と、最終archive一致確認後の4種でbuild／link／CPU実行を確認した。private Native guardはinstallしない。Impl定義と寸法生成入力は変更せず、生成fingerprintを維持している。今回の差分ではDebug／Release／cross寸法probeの再実行は行っていない。

初回の通常close試験では旧FBO保持によるweak失効2期待値の失敗を検出し、独立retention指定へ修正した。全体試験で検出した既存の取得中取消の回収遅延も修正し、最終重点／全体を再実行した。実driver故障、他OS、CPU並行操作、LSan、性能測定は未実施である。
