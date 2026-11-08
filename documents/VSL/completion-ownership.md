# Completionの結果とin-flight所有の分離

[完了計画](arena-completion-plan.md)の順序4。Completionを保存するだけでGPU資源を保持する構造を解消する。

## 所有と責務

`SubmissionImpl`はStatus／Failure／native error codeと、未完了時のweak Workを持つ結果。CompletionとCompletionObservationはこの同じ結果を共有する。terminalではweak参照も切り、保存した結果がmake_sharedのWork用control blockを保ち続けないようにする。結果からDevice・fence・資源を強所有しない。

`SubmissionWork`は結果を強所有し、Device／fence／依存資源のpinを持つ。PENDING時はPhaseと記録中のCommandBufferが保持し、送信成功後はCommandBufferのslot一覧が保持する。利用者が結果を保存しなくても、GPU使用中のWorkは失われない。最終所有者はdrainしてからnative資源を回収する。

operationは解決済みのnative Device／Fenceを受けてpoll／wait／idleを実行する。syncは結果を分類し、terminalを確定する。lifetimeは登録済みcallbackでCOMPLETION保持を解除する。結果を先に公開するため、資源の解放中にも終端が観測できる。結果とWorkの間に強い所有循環はなく、Frame自身がCompletionを保存しても成立する。

観測中は結果を局所的に保持し、解放callbackが呼出元のCompletionをリセットしても、その呼出しの結果を返す。wait_idleは開始時のWork／WSI ownerをsnapshotして回収する。回収callbackが新しく登録したJobとownerは次回の回収対象となる。

## 終端と失敗

- COMPLETE: GPU完了を確認した。`ready()`／`wait()`はtrue。
- CANCELLED: 未送信の記録を取り消した。`wait()`はfalse。
- FAILED: 生成／記録開始／送信準備・送信が失敗した、またはDevice lostを観測した。`wait()`はfalse。`failure()`と`error_code()`で理由を確認する。

`Failure::SUBMIT_FAILED`にはsetup・begin・送信前検証・end・fence reset・未受理のsubmit失敗を含む。Vulkanの失敗はnative result codeも保存する。CPU側検証等でnative codeがない場合は0。Device lostは`Failure::DEVICE_LOST`として区別する。既存Status値の番号は維持し、FAILEDを末尾へ追加した。

TIMEOUT／NOT_READYは終端ではない。その他の観測エラーもGPU完了と扱わず、例外を返してpinを保持する。destructorのdrainは例外を漏らさず、fence waitが確定できなければdevice idleへフォールバックする。それでも完了／Device lostを確定できない間は再試行し、資源を破棄しないため終了が待機し続ける場合がある。

setup／begin／end／reset／submitの失敗は、結果が生成済みの場合に共通のFAILED公開へ接続する。自身のrecording Workがある場合だけcommand bufferをリセットし、別のslotや送信を操作しない。準備中の完了通知が別Jobを開始した場合は、slotの使用状態とidentityを再検査し、その記録／送信を上書きせず準備を失敗させる。二度目のcancel／terminal通知で元の結果を上書きしない。

`ResourceUseState::wait()`は資源がidleになる待機であり、成功結果の判定ではない。FAILEDが使用一覧から除去され、trueを返す場合もある。処理の成功はCompletionで判定する。CPU側の並行操作には外部同期が必要。

## 保持policyと互換性

COMPLETION保持は完了・取消・送信失敗・Device lostのterminalで回収する。既存SUBMISSION保持はWork寿命まで維持し、slot置換やmanager破棄で回収する。結果がWorkを保持しないため、取消済みCompletionだけを保存して資源を延命する旧副作用はなくなる。引き続き資源が必要ならowning viewを保存する。

```cpp
auto view = vsl::BufferView::FromBuffer(buffer);
auto result = phase.completion();
phase.cancel();
// resultは取消結果だけを保持する。bufferの所有はviewが担う。
```

`Completion::retain()`はPENDING Workへ転送する移行adapter。CommandBuffer側の保持は記録中Workへ直接届き、結果からのweak解決を挟まない。公開data_の廃止は順序12で行う。

Completionの公開形状はshared_ptr一本を維持するが、CompletionObservationの配置は変更した。生成ABI fingerprintが更新されるため、VSL本体とconsumerを再buildする。古いバイナリとのABI互換は維持しない。

AHO Textureは未受理の失敗なら初期化観測を戻し、旧layoutを誤ってShaderReadOnlyと推測しない。Device lost後のupdateは拒否し、新しいDevice上での再作成を求める。ReadbackTicketは送信失敗とtimeoutを区別し、ticket自身によるslot／buffer／mappingの所有は維持する。

## presentationの暫定接続

graphics完了とpresentation完了は分ける。swapchain／present待ちsemaphoreはCommandBufferのWSI保持一覧へ登録し、通常のgraphics資源をその一覧へ混ぜない。present直前にも登録するため、setupとpresentの間のwait_idleで保持を失わない。

acquireではsemaphoreとともに常にacquire fenceを渡す。device／swapchain／available semaphore／fenceを所有するPresentationを、native acquire前にWorkへ保持する。通常のsemaphore経路ではsetupでCPU待機せず、最後の所有者がacquire fenceのsignal／Device lostを観測してから回収する。これはacquire後の送信失敗時にも、未完了signalの対象を破棄しないための処理。追加のacquire fenceは現時点では個別生成し、順序8で再利用単位を整理する。

acquire成功後にbegin／submitが失敗した場合、fence待機は安全な破棄に必要だが、binary semaphoreの消費や取得画像の返却は行わない。この失敗からの継続では当該semaphore／swapchainを再作成する暫定契約とし、再利用・画像返却の精密な復旧は順序10で扱う。通常cancelは従来どおり別Workでacquire semaphoreを消費し、元結果をCANCELLEDに保ったままpresentへ戻す。

WSI保持一覧はwait_idle／teardownまで保守的に保持する。ただし、追加拡張のないVulkanではDeviceWaitIdleをpresentation完了の証明にはできない。[Khronosの説明](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)に従い、順序10でmaintenance1のpresentation fenceまたはreacquireによる証明へ接続する。この単位でWSI全体の厳密な回収を完了と扱わない。

## 後続への条件

順序8でCommandBufferImpl自体をarenaへ配置する前に、in-flight registryを同じarena ownerのImpl外へ移す。Impl→Work→同arena rootの循環を作らない。明示的なFenceHolder／SemaphoreHolder::destroy()によるnative破棄の制約も順序8に残す。

結果stateとWorkは別control blockを持つ。従来の観測用Status単独の確保は廃止した。fence・資源一覧の個別確保も残る。CPU arenaのImpl配置を変える単位ではなく、性能改善率は未測定。

## 検証

Clang20／C++26、macOS arm64、ASan／UBSan、MoltenVK／Vulkan validationで確認した。追加CPU4件・VSL GPU7件・AHO GPU3件により、terminal公開と一度だけの解放、timeout／not-ready／未知エラー時の保持、Device lostの分類、保存した結果とWork／fenceの別寿命、Frame自身にCompletionを保存する場合、観測handleの回収中reset、回収中の次Job登録、準備中のpeer Job保護、Textureの初回失敗後の再試行とDevice lost観測時の拒否、readback失敗とslot再利用を検証した。

VSL760件中757成功／既知packed format3 skip、AHO GPU140・CPU1070、compile175件成功。変更C++26ファイル／19TUのstrict違反0。installed consumerで公開APIだけの構築・query・配置計画をbuild／link／実行。Debug／Releaseの実寸法assertと同寸法・異artifactのlink拒否、x86_64クロスコンパイルも確認した。

heap／arena両Typed描画サンプルで3frame／12submission・UBO更新・resize・pipeline生成1回を維持し、PNG3枚のバイト一致を確認した。CMake source path270件とMSBuild新規2pathの登録も確認した。

実Device lost、全native end／reset／submit／acquire失敗位置の注入、未知driverエラーでの回収再試行、CPU並行操作、Windows／Linux実機、性能は未検証。Device lostの追加テストは結果分類と利用側の拒否方針の検証であり、実GPUをlost状態にしていない。LeakSanitizerは無効化した。GPU資源の回収はweak ownerとvalidationで確認し、driver内部のGPUメモリ全量の解放を証明するものではない。
