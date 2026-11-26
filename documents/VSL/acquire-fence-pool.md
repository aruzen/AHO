# フレーム単位の取得Fenceと保存済みCPU proof

[完了計画](arena-completion-plan.md)の10dの一単位。従来の取得ごとのUniqueFence生成を、共通backendで構築したF個のnative Fenceへ置き換える。実画像数Mの[描画完了Semaphore](generation-semaphores.md)とは別の寿命群である。取得後失敗のTicket回収は後続へ残す。

## 構築と所有

CommandDomainは、初回のDefaultPhaseでResourceArenaのFENCE一枠を構築する。FenceCreateInfoのruntime countへcommand ringのFを渡し、生成した正規FenceImpl aliasだけを保存する。Computeだけなら生成しない。共通arenaの外側にpool専用owner、control block、使用予約表を追加しない。

storageの共通ResourcePlanがCPU Impl slotを計算し、lifetimeが構築・正規所有を管理する。operationは通常と共通のFence生成・reset・破棄を行い、syncのFenceUseStateがnative要素ごとの現在の用途を持つ。F個のhandle／使用状態の可変vectorはCPU Impl領域とは別確保である。

Presentationは元Fence ownerと取得に使ったslotを保持し、CPU-onlyのAcquireEvidenceからproofのaliasを渡して予約する。EvidenceはGPU ownerを持たず、世代もweak参照のままである。arena内部からDomainやarena自身への強い参照を作らない。

Domain破棄は先にWork／Presentationを回収し、その後にpool sourceを解放する。Presentationも取得済みFenceのterminalを保存してからsourceを外す。古いproofはreset後のnative handleを再観測しない。

## APIと使用経路

既存のDefaultPhase呼出しは変更しなくてよい。

```cpp
auto phase = commands.start_phase<vsl::DefaultPhase>(chain);
// 最初の呼出しでF個を構築。以降はframe_slotの要素を再利用する。
```

外部sourceを使う場合は末尾のoptional入力を指定する。このsourceをDomainのfallback cacheへ混ぜない。

```cpp
vsl::DefaultPhase phase(commands, chain, available, finished, graphics_fences,
                        false, acquisition_fences);
```

外部sourceの正規owner、Device、F以上の要素数を検査する。取得とgraphicsが同じnative Fence要素を使う入力はreset前に拒否する。同じarenaの別FenceImplは利用できる。

予約は共通reserve_acquire_fenceを使う。前の用途の完了を保存した後で新proofを登録し、native resetに失敗すると前の用途へ巻き戻す。解除はterminalだけでなく、同じproof pointerとcontrol blockであることを検査する。古いPresentationを後から破棄しても、再利用済み要素の新予約を消さない。

## callbackと取得失敗

別graphics使用の完了観測はowner解放callbackを呼び得る。局所Presentation／Work／Domain／sourceを先に保持し、予約後はPhaseCall、ring、slot、Work、世代を再検査する。自身が破棄されていればthisへ戻らず、peerの開始を検出した場合は外側だけを失敗させる。

native acquireの結果は局所Presentationへ先に保存し、その後にPhaseCallを確認する。成功／Suboptimalだけが取得indexとPENDING proofを採用する。失敗のoutputは無視し、NOT_ACQUIREDとして予約を解除する。native呼出中にPhaseが削除されても、実際に取得されたFenceのsignalを確認するまでsourceを保持する。

故障注入はprivate operationのスレッドscopeへ限定する。acquireとresetのC API結果を共通処理へ渡し、callbackやcontextをImplに保存しない。

## 検証と残る範囲

新8件でF個の反復再利用、Compute-only未生成、reset後の旧proof、旧Presentationの解除identity、Native失敗indexの不採用・再使用、reset OOMの巻戻し、外部source事前検証、同root別Fenceの寿命、予約callbackとnative acquire中の自己破棄を確認した。

macOS arm64／Clang20／C++26／MoltenVK1.4.2／ASan・UBSan・validationで、重点10件と全VSL1,075件が成功し、既知packed format3件だけskipした。AHO GPU160件／CPU1,070件、compile契約195件、strict9ファイル／5TU違反0、source370パス／エラー0も確認した。VUID／sanitizer errorはない。

installed public consumer26targetのbuild／link／CPU実行、末尾optional入力を使うDefaultPhase constructor、Debug／Releaseの寸法一致、x86_64 cross archive、異artifactのlink拒否とtemporary borrow負契約が成功した。Impl／slotの寸法は変えず、public phase.hppを既存fingerprint入力として再生成した。既存ソース呼出しは維持するが、公開型とconstructorのABIが変わるため、prebuilt利用側は再buildが必要である。

heap／arena両サンプルのPNG3枚が一致した。3frame・12submission・2draw・238831更新画素、1720×1080→2000×1360のresizeとpipeline生成1回を維持する。Windows／Linux実機、実DeviceLost、LSan、CPU並行、全native失敗位置は未検証である。

この単位では、成功取得後のbegin／submit／cancel失敗の画像返却を対象外とした。後続の[AcquireTicket／Surface回収](acquire-ticket-recovery.md)で、実signalの消費、画像releaseとRECOVERY_REQUIREDへ接続している。中断した世代を無条件に継続利用する契約にはしない。

PresentationのCPU確保、EvidenceのCPU確保、可変vector、Work／結果、既定graphics Fenceの所有は残る。Domain poolにより取得ごとのnative Fence生成はなくなるが、全体の性能率は未測定である。
