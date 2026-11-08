# Typedの配置・保持期間・固定依存の分離

arena既定化より先に、配置方式とは独立した寿命契約と管理レコードを整える。通常VSL／Typedの共通operation、owner graph、slot配置は維持する。

## 配置と記録保持

`BundlePolicy`はheap／arenaとbinding mapの選択だけを担当する。保持期間は`TypedResourceBundle`の独立した第3引数で選び、heap／arenaとも`COMPLETION`を既定にする。`TypedFrameResources`と`TypedDescriptorSet`の既定も同じに揃える。

```cpp
using Heap = t::TypedResourceBundle<Schema, t::HeapStorage>;
using Arena = t::TypedResourceBundle<Schema, t::ArenaStorage<>>;
// 両方ともGPU完了・取消・送信失敗等で、記録による保持を解除する。

// 旧Work寿命までの保持が必要な互換利用は明示する。
using OldRetention = t::TypedResourceBundle<Schema, t::HeapStorage,
                                           vsl::ResourceRetention::SUBMISSION>;
using OldArenaRetention = t::TypedResourceBundle<Schema, t::ArenaStorage<>,
                                                vsl::ResourceRetention::SUBMISSION>;
```

shared Setとframe Setへ同じ選択を伝え、Frame値自体の保持も揃える。保存したowning view／commandは、その所有期間まで資源を保持する。COMPLETIONは保存commandを無効化せず、同じcommandを別Phaseへ再記録できる。SUBMISSIONはGPU完了後もWorkの寿命まで保持する独立した明示選択であり、配置方式から推測しない。

既存の2引数Bundle宣言はそのまま使えるが、heapで完了後も記録の保持を期待していたコードは第3引数を明示する。単独`TypedFrameResources<Frame, SUBMISSION>`、TypedDescriptorSetのconstructor／FromResourceへの明示引数も利用できる。保存結果Completionは従来どおり観測だけを担う。

この変更の保持保証はbundle、descriptor、Frame値を対象とする。native generic commandとTypedの頂点／index／pipeline／pushにはSUBMISSIONの経路が残る。配置の比較では同じcommandを使う必要があり、全Typed資源を一律に完了直後回収すると主張しない。

## 固定schemaと所有graph

Bundleの第4引数は配置と独立した`DependencyPolicy`で、既定はFIXEDとする。

```cpp
using Fixed = t::TypedResourceBundle<Schema, t::ArenaStorage<>>;
using Editable = t::TypedResourceBundle<Schema, t::ArenaStorage<>,
                                        vsl::ResourceRetention::COMPLETION,
                                        vsl::lifetime::DependencyPolicy::DYNAMIC>;
```

構築中はDYNAMICでschemaの全bindingを接続し、公開前に実metadataの物理ownerと追加の論理constraint ownerを固定親へ移す。入力`descriptor()`を固定親抽出のために再評価しない。同root／固定親へのbinding参照は借用へ正規化する。frameから利用するshared側の外部ownerも、実際の接続を基に固定できる。

freezeは次のトランザクションである。

1. 外側rootを局所的に保持し、rootの更新gateを立てる。
2. 同じcontextの全Setについてvalid／recording／idleを検査する。idle観測のpoll callbackからの再記録・更新はgateで拒否する。
3. 既存固定親を保ったprepared vectorへ、全bindingとconstraintのownerを出所付きで重複排除し、cycleを検査する。
4. vectorのswap、spanの更新、FIXEDへの切替、既に固定したownerの借用正規化を行う。

検査・確保に失敗した場合はDYNAMICのまま、元の依存とbindingを維持する。FIXEDから新ownerを追加する更新はbackendの共通検査で拒否する。通常の記録でDFSは行わない。動的更新が必要なら第4引数を明示する。

ResourceArenaは同rootの全Setを`freeze_dependencies()`でまとめて扱う。heapのTypedDescriptorSetも同名の入口を持つが、単独Set専用contextに限る。共有contextを返すcustom ManagerのSetを一つだけ凍結する操作は拒否し、所有元ResourceArena全体の入口を使う。凍結解除は提供しない。

DescriptorOwnerContextは外側rootのvectorへの借用とreadonly spanを持ち、所有vectorや新root層を持たない。arenaは既存dependencies vectorを再利用する。heap Setの固定親は2要素arrayからvectorへ移したため、通常heapの親一覧にも個別確保が残る。このコストは最終監査で区別する。

## 管理レコード

ArenaResourceは15種類の参照フィールドを廃止し、型付きImpl pointerのvariant一つを保存する。referenceのサイズは2pointer以下をstatic_assertする。kindを検査したview取得後は、既存の解決済みImpl*を使って操作する。管理表の検索をVulkan操作ごとに追加しない。

ResourceId／kind／offset、外部owner index、Buffer部分viewのoffset／bytesは維持する。ownedの構築成功後の参照commitはnoexceptで、構築済み一覧への登録より前に例外経路を増やさない。publishで外部Implのnodeやcontextを上書きしない。同型slot、異種slot、外部部分view、逆順rollbackの既存経路も共用する。

macOS arm64の実sizeofでは、参照欄は従来の15pointer分120 byteから16 byte、record全体は72 byteとなった。vectorのcapacity、root、依存一覧等はこの値に含めない。総使用メモリや処理時間の改善率は未測定である。

領域先頭＋offsetからowned参照を都度復元する方式は今回導入しない。同じrecordに外部参照用pointerが必要なので、今回の型付きvariantだけで資源種ごとの参照欄増加を抑える。CPU配置領域、root／管理vector、生成scratch、Vulkan device memoryは別の確保である。

## 検証と所有取得

FrameResourceDomain::require_currentは既に保持するManager／ring／poolを借りて、元ring・control block・slot範囲と正規nodeを検査する。検査のための一時OwnerRefやCommandPool facadeを生成しない。descriptor正規化の自root判定も、保持を取得せずcontrol blockを照合する。

公開view取得・command構築・依存追加等で必要な所有は取得する。検査と所有取得を分けても、借用参照を保存する際の寿命保証は省略しない。

## 起動資源への適用

「起動時に作る」と「一緒に破棄する」を分ける。既定は共通Instance rootを先に公開し、Device群と各Window／Surface群を別ownerに置いてそれぞれimportする。Device生成時だけSurfaceを保持し、SwapchainがDevice群とSurface群の両方を保持する。

Deviceを使うBufferだけが残る場合、無関係なSurface／OS windowを保持しない。単一StartupArenaへ同居する構成は、寿命を揃えると明示した場合だけにする。具体的な起動APIは順序9で実装し、今回、未実装のStartupArenaが既にこの保証を満たすとは扱わない。

## 検証

Heap／Arena×COMPLETION／明示SUBMISSIONの組合せで、shared／frame Setの完了直後の回収、取消、旧Phase破棄と成功送信によるWork置換、保存commandの再記録、FIXEDの新owner拒否と明示DYNAMICを確認した。元のfacadeがpoll callback中に消える場合、freezeの再記録拒否・失敗後の同root再試行、別control blockを持つ不正aliasの拒否も検証した。

追加20件、重点202件、VSL982成功／既知packed-format3skip、AHO GPU150／CPU1070、compile契約195が成功した。native SUBMISSIONとTyped COMPLETIONが混在する記録でも、長い保持を短縮しないことを確認した。strictは変更18C++ファイル／11TUを差分ごとに検査し、修正後の違反0。CMake334pathとMSBuild／filtersの新private header各1回の登録も確認した。

installed consumer21target、Debug／Release、x86_64 compile-only／archive、異なるartifactのlink拒否、既存の一時値borrow拒否が成功した。17型の配置寸法と15kindの計画は不変。heap／arenaの実描画・resizeはPNG3枚一致、3frame／12submission／2draw、1720×1080→2000×1360、Pipeline生成1回を維持した。MoltenVK／Apple M1でvalidation／ASan／UBSanエラーはない。

実driverのOOM／DeviceLost、Windows／Linux実機、LSan、CPU並行利用、性能率は未検証である。既存の論理更新制約・循環検査・構築rollbackを変更対象と併せて回帰したが、すべてのdriver失敗を網羅した検証とは扱わない。
