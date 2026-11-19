# logical Windowとpluginの寿命境界

完了計画12fのWindow単位。起動arenaのOSWindowImplと、利用者が開閉するlogical Windowは別の寿命を持つ。

## 責務と参照

`PureWindow`は非公開WindowDataの所有値である。状態の定義・登録表・pluginの公開と回収・Updateの購読管理はprivate lifetimeへ置く。Nativeのpoll／resize／close flag／callback設定はoperationへ置く。OSWindowOwnerの既存arena配置とSurfaceの保持経路は変更しない。

通常のWindow操作は保存した状態へ直接届く。新しい所有wrapper／control blockは追加しない。Callはstack上の失効通知であり、状態を所有しない。facadeの破棄・move元・異なる値への代入／swapで失効し、plugin構築やAHO再作成処理はcallback後にCallを確認してからreceiverを再読する。開始済みの処理は状態・OS owner・描画世代等を局所値で保持する。

WindowIdentityは状態のアドレスと元control blockの弱い識別情報を組み合わせる。同じowner内の別slotを区別し、失効後のアドレス再利用も同一視しない。比較のためにownerをlockしない。

WeakWindowはlogicalな使用可否を表す。OSWindowOwnerやSurfaceがNative windowを保持していても、close後のlockは空になる。Device／Surfaceのweak値はstorageの生存を表すため、退役済みでもlockできる場合があり、契約は異なる。

## plugin登録

登録IDはWindowごとの登録表が管理する。Pluginの`requested_id()`は構築時の固定ID指定だけを返す。`nullopt`は自動採番、0も有効な固定IDである。公開の可変`Plugin::id`は廃止する。同じpluginオブジェクトのSurface ownerを交換しても登録先は変わらない。

登録表だけが登録pluginを強く所有する。更新順序はID列で保持し、dispatch時にIDとweak値のsnapshotを作る。呼出対象だけをlockし、同じID・実体・control blockの登録が残っていることを確認してからOnUpdateを借用する。外部が保持する置換前pluginを誤って呼び出さない。removeも同じ実体とcontrol blockを照合する。

追加・置換は更新順序の必要容量を先に予約し、登録表と順序をcommitしてから旧pluginを解放する。旧destructorが再入して再置換・削除・closeしても、外側が再び古いmetadataを書き戻さない。構築例外、ID枯渇、予約／map確保の例外では既存登録を保持する。plugin constructor中にreceiverが破棄・差し替えされた場合、または元Windowがcloseされた場合は公開せず、新pluginを回収する。close済みの登録先は構築前にも`std::invalid_argument`で拒否し、Surface追加の既存例外契約を維持する。

Updateはtick開始時のWindow一覧をweakでsnapshotする。listener一覧は各Windowの当番開始時に取得する。現在のWindowへ追加したlistenerと新Windowは次tick、後で更新する既存Windowへ追加したlistenerはそのtickの当番で実行する。削除済みをskipし、自己削除したlistenerは呼出終了まで保持する。close後はそのWindowの残りをskipし、他Windowの更新は続ける。GLFWCallScopeはdispatch全体を覆う。

HookShouldClose／HookResize／AHOのresize hookはstd::function自身も呼出前に値で保存する。callbackが自身を解除・置換しても実行中のcallableを破棄しない。空callbackは何もしない。

## 公開APIの移行

通常の生成・close／resize・`add_plugin<T>`の戻り値・型付きpluginの保持は維持する。raw WindowDataと共有constructor、WINDOWS、plugin map／更新列の公開を廃止する。

```cpp
vsl::Window window("example", 800, 600);
vsl::PureWindow copy = window; // 旧: PureWindow(window.data_)
auto weak = window.weak();
auto hook = window.add_plugin<vsl::window_plugin::HookShouldClose>(
    [](vsl::PureWindow current) { current.close(); });
auto id = window.plugin_id(hook); // 登録先WindowがIDを管理する
if (id && window.contains_plugin(*id, hook))
    window.remove_plugin(hook);
for (auto current : vsl::PureWindow::Snapshot())
    (void)current.name();
```

独自固定ID pluginは`Plugin(id)`をbase initializerで指定する。raw Native操作が必要な利用者は`auto os = window.window_owner()`を先に保存し、そのlvalueから`native_handle()`を借用する。raw pointerだけを取り出してownerを捨てない。

AHO製品はVSLのprivate状態を参照せず、PureWindow値／WeakWindow／公開plugin APIで接続する。RenderContextのlogical Window参照は弱くするが、AHO描画targetとEngineの強保持はこの単位に残る。Context全体が描画資源を延命しない保証は後続単位で扱う。

## 残る課題と検証

WindowDataのshared確保、plugin各個の確保、登録map／更新vector／dispatch scratch、callableの値コピーは残る。これらはCPU arena内のImpl配置やVulkan device memoryとは別である。AHO WindowData公開・可変base継承・facade全値の破棄／代入開始時のCall失効・InputManager自体のcallback中破棄は次のAHO単位で閉じる。現在のbase Callだけでは、AHO member回収中の再入を含む全値の保証は完了していない。Completion公開結果と実Impl定義の4層分割も未完了である。

既存の登録枯渇・wrap・固定ID置換・Update中追加削除・close回収・Swapchain失敗の試験に、同owner別slot identity、closed weak、構築中receiver破棄／差し替え、旧plugin解放時の再置換、callable自己解除／置換、AHO framebuffer query中receiver破棄／差し替え、公開API正負契約を追加する。実行結果はPRの検証記録へ記載する。性能改善率は未測定である。

実行検証ではVSL 1,207件（既知format制約3件skip）、AHO GPU 203件・CPU 1,071件・入力46件が成功した。MoltenVK／validation／ASan／UBSanで確認し、heapとarenaのTyped描画PNG3枚は一致、resize後のpipeline生成は1回、glTF viewerのverifyも成功した。関連compile388件と公開Window正負10件、ソース登録411件、strict規約検査も成功した。

install後は公開28ファイルのcompileとリンクした利用例の実行、Window正負10件、非公開定義が配布されないこと、公開header／生成header／archiveの一致を確認した。Debug／Releaseとx86_64の配置情報を実定義で検査し、異なる構成または親PRの生成情報を混ぜるlinkはABI guardで拒否された。
