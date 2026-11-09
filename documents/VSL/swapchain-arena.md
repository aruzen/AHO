# Swapchain世代の共通backendとCPU配置

順序10b。通常の`Swapchain`と`lifetime::SwapchainArena`を、同じnative生成・画像view構築・指定領域構築へ接続する。graphicsとpresentationの厳密な完了証明、AcquireTicket、AHOのattachment失敗からの復旧は10c〜10eへ続く。

## 4責務と配置

`SwapchainArena`の末尾の任意引数`SwapchainCreateProgress*`は、生成中だけ借用する。共通backendは入口で`native_started=false`へ戻し、入力／能力／旧世代の全検証後、`vkCreateSwapchainKHR`の直前にのみtrueへ進める。例外後にも値を保持するため、旧世代がない場合もpreflight失敗とNative開始後の失敗を区別できる。trueは生成成功の保証ではない。この値をImplやownerへ保存せず、再入／並行呼出しで同じ出力領域を共有しない。

```cpp
vsl::SwapchainCreateProgress progress;
try {
    vsl::lifetime::SwapchainArena next(device, surface, info, old, {}, &progress);
    publish(next.view());
} catch (...) {
    if (progress.native_started)
        remember_validated_request(info);
    throw;
}
```

既存の省略引数による生成は共通処理を使い続ける。公開constructorのリンク先変更を含むため、prebuilt利用側は再buildする。

- operation：raw Device／Surfaceと生成入力を検証し、native Swapchain／ImageViewを生成・破棄する。成功したnonnull handleだけを採用する。
- storage：実画像数MからSwapchain slotとImage slot×Mのoffset・全体サイズ・alignmentを計算し、領域を提供する。
- lifetime：native生成中のPending、構築と巻き戻し、Device／Surfaceの固定親、世代ownerと公開aliasを管理する。
- sync：全aliasで共有する現役／退役状態、既存の画像物理UseState、記録と送信の完了を扱う。取消時の内部使用登録は、過去の完了callbackを発火させない。

世代rootはCPU領域と正規親ownerを持つ。配置されたSwapchainImplはraw親、native handleと値、画像slotの借用rangeを持つ。ImageAccessorは同じrootのaliasである。rootやImplから、自身のImageAccessorを強く所有しない。

ImageImplのUseStateは既存slotに一つだけ配置する。同じ画像の通常記録と取消返却が、この状態へ登録する。通常操作は解決済みImplとnative handleへ直接届き、画像rangeもID探索を行わない。Device／Surface getterは固定親の正規ownerから元のcontrol blockを取得する。

今回の一括領域はCPUのSwapchainImpl、ImageImplとそのslot内状態を含む。native画像のVulkan内部メモリ、native画像一覧・公開ImageAccessor一覧・生成scratchのvector、同期用native objectと内部vectorの確保は別である。通常入口もこの配置を使うため、画像ごとのHeapImageOwner確保を行わない。

## 実画像数とABI

Mは`vkGetSwapchainImagesKHR`の結果であり、要求したminImageCountやframe数Fではない。列挙がVK_INCOMPLETEなら再取得し、zero／範囲外count／null画像を拒否する。native生成後に`SwapchainLayout(M)`を作り、必要なCPU領域を取得する。

Impl完全定義はprivateに保ち、既存のcompile-only寸法生成へSwapchain Impl／slotを加える。対象・compiler・設定に対応した生成ヘッダー、実定義とのstatic_assert、install exportとlink ABI検査を使用する。公開Arena constructorも利用側でABI検査を呼び、利用者がLayoutを明示生成しない場合のartifact混在を拒否する。

## 構築と不可逆な退役

1. canonicalなDevice／Surfaceと旧世代を検査し、生成呼出し中の所有を確保する。寸法・usage・queue・present policy等の不正入力はnative前に拒否する。
2. native create直前に旧世代をRETIREDへ変更する。全aliasが同じ状態を見る。
3. native成功後はPendingが新handleと実画像Mを持つ。列挙・CPU確保・後続構築が失敗すれば、新handleを回収する。
4. CPU領域をclaimし、借用native画像からImageImpl／ImageViewを順に構築する。途中失敗は成功済み画像を逆順に破棄し、自分がclaimした領域だけをreleaseする。
5. SwapchainImplへnoexceptで採用し、rootと全slotのnodeを公開する。最後に外側のImageAccessor aliasを返す。

nativeへoldSwapchainを渡した後は、生成失敗でも旧世代をACTIVEへ戻さない。CPU構築のrollbackと、WSIの不可逆な退役は別の契約である。再試行では退役handleを再指定しない。[oldSwapchainの契約](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainCreateInfoKHR.html)

生存中の画像view一つでも世代領域全体を保持し、同じ領域の再claimを拒否する。最後のaliasが外れた後は、ImageView群→native Swapchain→CPU領域→親の順に回収する。

## 通常入口と明示配置

```cpp
vsl::Swapchain chain(device, surface, {}, 640, 480);
auto image = chain.images.front();

vsl::SwapchainCreateInfo info{.width = 640, .height = 480};
vsl::lifetime::SwapchainArena arena(device, *surface, info);
auto view = arena.view();
```

再利用可能なCPU領域はfactoryで渡せる。factoryは実M確定後に一度だけ呼び、rootへ保存しない。

```cpp
std::optional<vsl::storage::ArenaMemory> memory;
vsl::lifetime::SwapchainArena arena(device, *surface, info, {},
    [&](const vsl::storage::SwapchainLayout& layout) {
        if (!memory)
            memory.emplace(layout.bytes(), layout.alignment());
        return *memory;
    });
```

画像数増加などで容量が不足する場合は、factoryが新しい領域を用意する。生存中の世代領域の拡張・移動は行わない。

## 記録・取得・取消と互換

Phaseは取得対象世代をsnapshotし、準備callbackの後とnative acquire直前に同一性・ACTIVEを再検査する。取得したindexは内部Presentationへ保存し、公開`swapchain`／`image_index`が書き換わっても、返却・presentは実際に取得した世代とindexを使う。

取消の返却Workは、barrier記録前に世代とPresentationを保持し、同じ画像の物理UseStateへPENDINGを登録する。内部登録は保存状態だけで検査・整理し、古いWorkをpollしない。通常のmark_used／busy／waitでの完了観測は維持する。これにより、返却barrierの途中で過去の完了callbackが現Phaseを破棄する新しい再入点を作らない。

取得情報のPresentationは共通lifetime処理でWorkのcompletion_resourcesへ保持し、GPU完了／取消の終端で解放する。Presentation自身が実世代を所有するため、同じWorkへ世代を重複登録しない。公開`Completion::retain`の旧SUBMISSION契約は維持する。presentation側の保持は引き続きDomainに残り、graphics完了だけでは解除しない。

AHOの最小復旧は、現世代が退役済みならoldなしで再生成し、明示present policyを次世代へ引き継ぐ。attachment生成後の失敗・zero extent・frame数Fと画像数Mの同期群分離は10eで扱う。

公開`data_`とImageAccessor一覧は順序12までの互換入口である。通常Swapchain constructorも互換adapterとして共通Arena入口へ委譲する。現時点のlegacy Domain保持やacquire Fence個別生成は、10c／10dの移行対象として残る。`REQUIRE_FENCE`のfeature有効化だけで厳密なpresent回収を保証しない。

## 検証状況

新VSL11件とAHO1件を追加した。配置・共通物理状態・元親owner・画像aliasの寿命・領域再利用拒否、native列挙／途中ImageView生成の失敗と巻き戻し、失敗出力の未採用、不可逆な退役、退役handleを省く再試行を確認した。準備callbackの世代退役／差替え、公開値を書き換えた後の取消、明示policyのresize継承も検証した。

取消時に通常のpoll付き使用登録へ戻した比較実装では、新回帰がPhase自己破棄後の不正アクセスをUBSanで検出した。修正後は成功する。取得情報をWork寿命まで保持した状態で失敗した世代回収テストも、COMPLETION保持への整理後に成功した。

- 重点VSL27件／AHO48件、拡大重点VSL159件、全VSL1,040件、AHO GPU159件／CPU1,070件、コンパイル契約195件が成功。
- Apple M1／MoltenVK1.4.2、validation・ASan／UBSanで確認。skipは従来packed integer clearのdriver制約3件のみで、新ケースのskipなし。LSanは無効。
- 規約34ファイル／19TUと保持処理の補足3ファイル／2TUで違反0。source362件違反0、新8製品pathをCMake／MSBuild／filtersへ一意登録。
- Swapchain Impl／slotは160 byte、alignment8。20型traits、ResourceKind15種とStartupKind4種を維持。install consumer26target、Debug／Release、x86_64 compile-only／archive、artifact混在link拒否、直接Arena構築でのABI負例、一時値借用の負例が成功。
- heap／arenaは3frame／12submission／2draw、resize1720×1080→2000×1360、Pipeline生成1回。PNG3枚が対応ごと一致した。

他OS、実driver OOM／device lost、複数queue family実機は未検証。present Fenceのsignal／厳密回収は今回の検証に含まない。性能改善率は未測定であり、画像ごとのHeapImageOwnerを省いた構造と、描画結果の維持を確認した段階である。次は10cの外部PresentRecordと完了証明へ進む。
