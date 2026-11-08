# Phase準備中の借用寿命

順序10dの前半。後続のAcquireTicket／取得後失敗の回復より先に、準備中の完了callbackがPhase自身を破棄する経路を扱う。

## 問題と責務

ComputePhaseのprepareは前slotのCompletionをwaitする。COMPLETION保持を外したときのdeleterは、現在のpostponed Phaseを破棄できる。別ringの外部Fenceを予約する場合も、旧Workの完了観測から同じcallbackが起きる。Domain／Workのownerをpinしても、借用thisの寿命は保証できない。

private lifetime::PhaseCallは、呼出しstackに置くguardである。Phaseはactive guardの先頭だけを借用し、guardは前guardとPhaseの借用を保存する。Compute／Default destructorは最初に全active guardを失効する。失効済みguardのdestructorはPhaseへ触れない。新しいshared owner、control block、guard用のheap確保は追加しない。

このguardは同期的なcallback後の生存検査であり、Phaseを延命しない。外部直列化の契約を維持し、並行deleteを許可するものではない。通常Vulkan操作に所有解決やregistry探索を加えない。

## 準備・例外・再入

prepareはprevious waitと外部Fence予約の後でguardを確認し、失効ならthisを読まずfalseを返す。setup／取消返却の呼出側もouter guardを持ち、prepareの局所owner破棄後の失効を確認する。catch側も失効後はfail(this)を再実行しない。

guardは局所ownerより先に宣言し、ownerの破棄が終わるまで存在させる。失効フラグを書き換えるため、guard自体をconst objectにしない。生存時はLIFOで先頭を元へ戻し、destructor中に失効した場合は戻さない。

既存の「完了callbackがpeer Phaseを開始し、外側prepareだけが変更を検知して失敗する」動作を維持する。callbackを準備終了まで遅延させたり、新しい所有層へPhaseを包んだりしない。今回の対象は標準Compute／Defaultのprepare・setupと取消の呼出境界である。

## APIとABI

通常のconstructor／setup／submit／cancelの呼出形式は維持する。protected prepareは`[[nodiscard]] bool`へ変更するため、独自派生クラスで直接使う場合は中断を検査する。

```cpp
if (!prepare())
    return;
begin();
```

標準setupはprivate guardで呼出側の後処理も保護する。独自override内部の任意callbackや、寿命切れ後の独自this参照全体を自動保護するAPIではない。

公開ComputePhaseにprivate pointer一つが増えるため、headerとlibraryを一緒に更新する。既存artifact検査のpublic入力へvulkan/phase.hppを加え、生成情報のhashとconfigure依存へ反映する。Implの配置寸法は変わらない。guardの完全定義と実装はprivateであり、installへ公開しない。

## 検証

旧実装では、ComputeとDefaultのpostponed setupを完了callbackから破棄する2件がUBSanのinvalid vptrで停止した。修正版では同ringのprevious wait、別ringの共有Fence予約、Defaultのnative acquire前の中断を確認する。中断後も次Phaseで同じFence／Command ringを使えること、既存peer-startと世代差替え検査を維持することを検証する。

Apple M1／MoltenVK1.4.2、Vulkan validationとASan／UBSanで、新2件と既存peer-start／世代変更／取消／presentationを確認した。全VSL1,058件（従来driver制約skip3）、AHO GPU159件／CPU1,070件、コンパイル契約195件が成功した。新規skipなし、LSanは無効。

規約5ファイル／3TUと、拡張した共有Fence testの補足1ファイル／1TUで違反0。source370件の違反0、CMake／MSBuild／filtersへの新2pathの一意登録を確認した。install consumer26target、Debug／Release、x86_64 compile-only／archive、混在artifact／直接Arena構築のlink拒否、借用負例が成功した。新しいprivate guardはinstallしない。Impl寸法を維持し、公開Phaseを含むABI入力を更新した。

heap／arenaの3frame／12submission／2drawとresizeを通し、Pipeline生成1回とPNG3枚の一致を維持した。他OSと実DeviceLostは未検証。AcquireTicketとFence pool、取得後のbegin／submit／cancel失敗時にsignalを消費して画像を処理する経路は、この変更だけでは解決しない。順序10d後半へ続く。
