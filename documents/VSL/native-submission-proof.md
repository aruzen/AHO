# 送信結果とnative使用完了の分離

[完了計画](arena-completion-plan.md)の10dで、AcquireTicketを接続する前提となる変更。送信・完了・回収の経路を実装し、以下の環境で検証した。

## 契約

公開Completionの成功・取消・失敗と、実際のqueue enqueue・native使用完了を別々に保存する。`FAILED`だけを理由にBuffer更新、command bufferのreset、Fence再利用、owner解放を許さない。

| Native submit結果 | enqueue分類 | 資源の使用完了 | 公開結果 |
| --- | --- | --- | --- |
| SUCCESS | ENQUEUED | Fence／Device idleを待つ | SUBMITTED→COMPLETE、またはDEVICE_LOST |
| host/device OOM | NOT_ENQUEUED | この送信による使用なし | FAILED |
| DEVICE_LOST | DEVICE_LOST | 損失として確定 | FAILED |
| その他 | UNKNOWN | 不明。Fenceのpoll／Device idle／損失で確認する | FAILEDを維持 |

[vkQueueSubmit](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit.html)のOOM時の不変条件を利用する。未分類の結果を未enqueueと推測しない。UNKNOWN後にidleを確認しても、元のFAILED／error codeを成功へ書き換えない。

`Completion::busy()`は未送信記録、Native transaction中、または未完了native使用を表す。`native_uncertain()`はUNKNOWNという履歴を表し、idle後もtrueである。`ready()`／`wait()`は処理の成功だけを表す。UNKNOWN Fenceは送信されていない可能性があるため、公開Fenceの待機もpollに限定し、無期限waitしない。

## 4責務と所有

- sync: enqueue分類、native完了、busy、immutableな公開結果、同じ物理Semaphore要素に残るpoisonを管理する。
- operation: 解決済みのqueue／command buffer／Fenceに対するC Vulkan呼出しと、その実結果を返す。所有を取得しない。
- lifetime: Workの登録、Native中の再入保護、UNKNOWN Workの退避、全観測値の保存後の回収を担当する。
- storage: Implと隣接contextのCPU配置を維持する。今回の変更はVulkan device memoryの一括化ではない。

SubmissionImplはCPU結果とweak Workだけを持つ。ResourceUseStateがCPU結果をstrongで観測しても、GPU ownerへ強い参照は戻らない。通常のBufferはnative完了後に再利用できる。AHO Textureの論理stateは`REQUIRE_RECREATION`を選び、UNKNOWN履歴を残して古いlayout／初期化状態の推測を禁止する。読み取り使用のUNKNOWNも再作成を求める保守的な契約である。低位VSL Imageの明示layout管理と区別する。

strictなstateを記録したWorkは、解決済みstate参照を送信直前にも検査する。stateの元ownerが既存資源ownerと別なら保持し、同じcontrol blockなら重ねて保持しない。既に記録済みのdescriptor制約も、後から発生したUNKNOWNを認識する。

UNKNOWN Workは外部のintrusive headへ退避する。Work自身のcontrol blockを使い、新しいEntryや世代ownerへ包み直さない。command ringの正規ownerが既存保持一覧になければ、専用のshared_ptr欄にだけ保持する。Native失敗後にvectorの追加確保を要求しない。Work→資源rootの一方向で、arena内部→外部Workの強い参照を置かない。

## Native transactionと回収

end／Fence resetからsubmit結果の採用、元slot／contextへの公開までを一つのtransactionとする。その途中の同Phaseの取消・破棄はabandon要求だけを記録し、transactionは実結果まで完遂する。二重submit、記録済みCBへの追記、retain、同ringのreset／next／wait_idleは拒否する。Phaseが破棄されても局所Work／Domain／native参照で結果を採用する。presentはgraphics Resultのactiveを流用せず、保持中Presentationの別gateで同Phaseの再present／cancelを保護する。独立Domainのqueue全体を排他する仕組みではなく、CPU並行操作の外部同期契約は変えない。

streamは開始時のCompletionを保存し、holder callbackの前後で同じ記録を検査する。空のCompletionを指定する文脈検査は、Phaseなしの既存CPU metadata commandを許すが、記録・Native transaction・変更中のringを許さない。引数なしの`require_recording()`と資源の記録は引き続き有効なPhaseを必須とする。この互換入口は、PhaseなしでVulkan記録を開始するAPIではない。

回収は開始時のheadをdetachし、対象scopeの全観測値保存、head／未回収数の確定、lock外でのowner解放の順に行う。callbackから登録された新Workは次回へ残す。snapshotが例外で戻された場合も、証明済みWorkのCompletion観測から回収を再試行できる。

headless WorkはDevice単位、Windowに属するWorkのowner解放はSurface単位とする。既知DeviceLostは同DeviceのCPU観測値へ先に保存するが、別Surfaceのownerを任意のWindowの回収で外さない。元Domainを失ったringへattachする場合は、同じ正規ring・元slotの未確定Workを引き継ぎ、idleまでresetを禁止する。

UNKNOWNでwait／signalしたbinary Semaphoreの要素にはpoisonを残す。idleはsignalが消費されたことまでは証明しないため、完了後も再利用を拒否する。使用完了後のdestroyは許す。Fのwait indexとMのsignal image indexを区別し、present直前にも保存した元sourceの要素を検査する。

## 残る範囲

後続の[AcquireTicket／Surface回収](acquire-ticket-recovery.md)で、取得済み画像の返却、known signalの一度だけのdrain、RECOVERY_REQUIRED、元Domain／Completionを失った後の回収入口を接続した。AHO resize／closeは10eに残り、現時点でWindowの失敗復旧全体を完了とは扱わない。

Work／Result、内部vector、同期観測scratch、Semaphore要素別poison配列には個別CPU確保が残る。strict validationの参照一覧も可変長である。通常操作は取得済みImpl／nativeへ直接届き、UNKNOWN退避一覧を毎回探索しない。性能改善率は未測定であり、追加metadata・CPU結果のstrong観測も最終計測の対象とする。

## 検証

分類、UNKNOWNの実送信あり／なし、完了前保持、idle後の回収、元ringへの引き継ぎ、snapshot失敗／callback再入、取消・自己破棄、Textureの再作成要求と旧binding、要素別poisonを、新VSL21件・AHO1件を含む検証で確認した。元Workと外部Completionを破棄してもCPU履歴だけが残り、GPU ownerを保持しないことも確認した。

Clang20／C++26、macOS arm64／Apple M1、MoltenVK1.4.2、ASan／UBSan／Vulkan validationで、全VSL1,096成功・既存driver制約skip3、AHO GPU161・CPU1,070、compile契約195が成功した。heap／arenaのPNG3枚は対応ごとに一致し、resize後のPipeline生成1回を維持した。install consumer26種で新公開APIのbuild／link／CPU実行、Debug／Release実寸法、x86_64 archive、異artifact／一時借用の拒否も確認した。規約検査は変更36ファイルと代表TUを検査し、宣言順修正・未解析ヘッダー補完後の再検査で違反0。source path370件の違反0である。

callback後の再検査を一時的に外した比較では、CPU回帰2件が期待どおり失敗した。正しいsourceへbyte一致で復元して再buildし、関連CPU10件が成功した。Phaseなしのmetadata streamを使う既存3件も期待値を変更せず維持した。

実driverのOOM／DeviceLost、他OS、複数queue family実機、CPU並行操作、性能は未検証。故障注入で実送信した場合は実queue完了を確保した後、CPU観測だけを遅延させるため、回帰時にも使用中native資源を破棄しない。これは実GPUをlost状態にした検証ではない。LeakSanitizerは無効化した。

ResourceUseStateはこの環境で24→32byte、SemaphoreImpl／slotは112／128→136／152byte、CommandBuffer slotは120→128byteとなった。必要な状態と回収counterの追加であり、メモリ削減とは主張しない。生成artifactを更新しているため、prebuilt利用側も再buildする。
