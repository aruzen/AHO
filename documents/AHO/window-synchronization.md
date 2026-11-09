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

DrawPhaseの構築は一度取得したWindowから、Swapchain・F/M同期・RenderPass／Framebufferを参照する。以降のcontextやresizeまで対象を固定する変更、0 framebuffer時の休止、未公開candidateの再試行、closeの統合は10eの次の単位に残す。今回だけで10e全体の完了とはしない。

## 検証

新GPU7件／CPU1件と既存strict policyの拡張で、F群の同root／別Impl、Engine生存中の回収、同名Windowの独立、resizeでのF保持とM交換、初回／追加Window／overrideのpolicy、取得source省略の互換、Window一回取得、null／未起動Engine拒否を検証した。

macOS arm64／Clang20／C++26／MoltenVK1.4.2、ASan／UBSan／Vulkan validationで、重点GPU9件、AHO GPU全168件／CPU全1,071件、compile契約195件が成功した。heap／arenaのPNG3枚は一致し、3frame／12submission／2draw／238831更新画素、1720×1080→2000×1360のresize後もPipeline生成1回を維持した。

変更library10ファイルを初回6TUと修正後のCPU／GPU variant補完で検査し、規約違反0。サンプルMainはformatterと実buildで検証した。source path376件もエラー0。installed public consumer3種（既存2＋新Window API）がbuild／link／CPU実行に成功した。VSL backend／実寸法入力は変更していない。

policyを従来のLEGACY固定へ一時的に戻した比較では、初回・追加Window・resizeの3期待値で回帰1件が失敗した。byte一致で復元し、全target再build後に同1件が成功した。実driver故障、他OS実機、CPU並行、LSan、性能は未検証であり、改善率は断定しない。
