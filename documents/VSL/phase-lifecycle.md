# Phaseの画像番号・同期・寿命

RenderPass構成APIのPR #56に続く修正。frame slotはCommandBuffer・acquire用Semaphore・完了Fenceの添字、imageIndexは取得したSwapchain画像・FrameBuffer・present用Semaphoreの添字として扱う。

- `DefaultPhase::getCurrentIndex()` はframe slot、`getImageIndex()` は取得画像番号。取得前のimageIndexはUINT32_MAX。
- 旧RenderPassBeginも取得画像番号を参照する。新Beginの明示targetIndexはそのまま優先する。
- present用SemaphoreはSwapchain画像単位で用意する。ユーザーが渡すcalculationFinishも実画像枚数分が必要。省略時はSwapchainが保持するSemaphoreを利用する。
- acquire用Semaphoreを省略した場合は、取得FenceをCPUで待ってから記録する。SemaphoreとFenceの両方をnullにしてacquireする経路はない。
- Phaseは前回同じCommandBufferを使用した送信の完了を待ってからresetする。外部Fenceを省略しても内部Fenceで管理する。Fenceをresetするのはacquire成功後のsubmit直前。
- `ComputePhase` のSemaphore待機stageは既定AllCommands。末尾のwaitStage引数でTransfer / ComputeShader等を指定できる。
- present前にCPUで全Fenceを待たない。画像ごとのrender-finished SemaphoreでGPU / presentation間を同期する。
- `Completion` は描画・転送の一つの送信を識別し、presentationの完了は表さない。Swapchainやpresent用Semaphoreの破棄前には別途完了管理が必要。Pending / Submitted / Complete / Cancelledを区別し、native Fence再利用後も古い完了結果を維持する。Pending / Cancelledのwaitはfalse。
- `Completion::retain()` は送信前にGPUリソースを保持する。RenderPassBeginはFrameBufferを保持する。任意の描画コマンドすべてのリソース保持を自動化したものではなく、必要に応じて利用側もretainする。
- Phaseのcopy / moveを禁止し、同じmanagerへの入れ子の記録を拒否する。同じqueueやCompletionへの並行CPUアクセスは利用側で同期する。

## 明示的な送信とpresent

```cpp
auto phase = manager.startPhase<vsl::DefaultPhase>(swapchain);
const auto imageIndex = phase.getImageIndex();
// 対象画像へのRenderPassと必要な読み戻しコマンドを記録する。
auto completion = phase.completion();
phase.submitCommands();
phase.present();
// CPUで結果が必要になったときにcompletion.wait()する。
```

既存のsubmitはsubmitCommandsとpresentを順に実行する。デストラクターの自動送信も維持する。acquireでOutOfDateなら記録を開始せず、submitはWantRecreateSwapchainを返す。Suboptimal時は取得画像を送信・presentしてから再生成を要求する。

記録中のComputePhaseはcancelでき、送信せずCompletionをCancelledにする。DefaultPhaseのcancelは部分記録を捨てたうえで、取得済み画像を空の送信・presentで返す。破棄時の例外巻き戻しでもこの処理を行う。submit後のcancelは既に送信した仕事を取り消さず、未実行のpresentを完了する。

## 寿命管理の修正

- CommandPoolは最後の所有者で破棄する。CommandBufferは使用した送信の完了後に解放し、その後poolを解放する。
- 同期HolderからSynchroManagerへの参照はweak、deviceへの参照はsharedとする。HolderはManagerより長生きでき、Managerとの循環所有を作らない。
- native Fence / Semaphoreは最後のHolderで解放する。明示destroyは対応する登録を安全に除去し、配列を空にするため繰り返し呼び出せる。同名の新しいHolderを古いHolderのdestroyで消さない。
- GPUが参照中の同期オブジェクトの明示destroyは呼び出し側が避ける。
- AHOのDrawPhaseから全Fence待ちを除去。Windowリサイズ時はdeviceの処理完了を待ち、present用Semaphore数を新しい画像数へ合わせる。

## 検証と既存PRとの分担

取得画像の選択・読み戻し・presentを複数フレームで確認し、取得済みフレームのcancel、無関係な未signal Fenceを待たないこと、Completionの保持とFence再利用、同期Holderの寿命と繰り返しdestroyをValidation Layer付きで検証する。

最初の検証ではAHOの描画assertionは成功したが、同期オブジェクトの循環所有でVulkan deviceが残り、Validation Layerの終了処理で停止した。上記の所有修正はこの調査に基づく。同じ終了hangのあるテストを繰り返す代わりにスタックを採取した。

PR #78〜#80のBuffer修正、#87のCommandBuffer::next実装、#88のコマンド基底デストラクター修正等は別PRの範囲とし、差分を重複させない。

present用Semaphoreの画像単位での再利用は[Khronosの説明](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)、Fence resetの順序は[Swapchain再生成の説明](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html)に従う。拡張なしVulkanにおけるpresentation完了・Swapchain終了時の厳密な寿命管理には既知の制約があり、本修正はswapchain-maintenance拡張の追加実装を含まない。

## 2026-10-18 検証結果

Apple M1 / MoltenVK、Apple Clang 17、Vulkan SDK 1.4.309.0、Debug。関連targetの再ビルド成功。VSL全体とAHOのReadback / DrawPhaseおよびAHOヘッダーcompileで208件成功、失敗0、環境skip 0。既存26件はDISABLEDのまま。AHO DrawPhase 3件はassertionだけでなくプロセス終了まで成功した。新しいPhase・同期・Swapchain読み戻しのテストはValidation Layerを有効にした。Windows / Linux実行は未検証。

```sh
ctest --test-dir build-vsl-phase --output-on-failure \
  -R '^(VSL\.|AHO\.(ImageReadback(Busy)?VulkanIntegrationTest|DrawPhaseVulkanIntegration)\.|AHO\.Compile\.Header\.image_readback\.hpp$)'
```

Completion追加と内部所有構造の変更により、VSL / AHO / 利用側を同じヘッダーで再ビルドする必要がある。
