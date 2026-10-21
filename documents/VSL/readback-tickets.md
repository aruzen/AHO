# AttachmentId指定とReadback Ticket

PR #56 → #105 → #109に続く③の変更。既存の配列順指定とcapture / ready / wait / readを維持し、Attachment追加時の取り違えと非同期結果の対応付けを改善する。

## Attachmentの結び付け

```cpp
RenderPassDescription description;
const auto color_id = description.add_color({
    .format = color.get_format(), .final_layout = ImageLayout::GENERAL,
});
const auto object_id = description.add_color({
    .format = ids.get_format(), .final_layout = ImageLayout::GENERAL,
});
RenderPass pass(device, description);
auto target = FrameBuffer::FromAttachments(pass, width, height, {
    {object_id, ids}, {color_id, color},
});

phase << command::RenderPassBegin::WithClears(target, {
    {object_id, ClearUint{}}, {color_id, ClearColor{0, 0, 0, 1}},
});
```

画像とCLEAR値の記述順は問わない。FromAttachmentsは全Attachmentの結び付けを要求し、不足・重複・範囲外を拒否する。WithClearsは一部指定も可能で、省略したAttachmentにはformatに対応する既定CLEAR値を使う。重複・範囲外・CLEARが実際に使われるAttachmentの型不整合を拒否する。

AttachmentIdはdescription.attachmentsの添字。shader出力locationはsubpass.colorsの位置なので、Depth等を追加した場合も両者を混同しない。配列版コンストラクターを増やして曖昧にせず、名前付きfactoryを追加した。既存の `FrameBuffer(..., {})` / `RenderPassBegin(target, {})` の解決は変えない。

## 既存Phaseへ読み戻しを記録する

```cpp
aho::ImageReadback reader(manager, 0, 2); // 読み戻し用クラスを保持し、結果用スロットを最大2件使う。
std::optional<aho::ImageReadbackTicket> picking, screenshot;
{
    auto phase = manager.start_phase<vsl::ComputePhase>();
    // RenderPassBegin、描画コマンド、RenderPassEndを記録する。
    picking = reader.record(phase, ids, vsl::ImageLayout::GENERAL,
                            {x, y, 1, 1}, frame_id);
    screenshot = reader.record(phase, color, vsl::ImageLayout::GENERAL, {}, frame_id);
    // 2件の読み戻しを描画と同時に送信する。record内では送信やCPUでの完了待ちを行わない。
}
if (picking && picking->ready()) {
    picking->read(pixels);
    const auto frame = picking->info().frame_id;
    picking.reset(); // 結果を回収したらスロットを返す。
}
```

readback helperは1つでよい。recordだけ使う経路では独自のCommandPool / CommandBuffer / Fenceを作らず、渡したPhaseのCompletionを使う。複数の転送を一つのPhaseに記録すると同じCompletionを共有する。

[完全なc++使用例](examples/readback-tickets.cpp)は、カラーと整数IDの同時出力、AttachmentId指定、描画オブジェクトの保持、2組の描画CommandBuffer再利用、1px IDとスクショの読み戻し、readyによる回収を示す。受理された要求のframe_idを確認する構成で、すべてのフレームの要求が必ず受理されるという例ではない。

## Ticketとslotの契約

- `capture_ticket(image, layout, region, frame_id)` は専用の再利用managerへ転送を送信する。producerは同じgraphics queueへ先に送信しておく。
- `record(phase, ...)` は記録中の同じdeviceのPhaseを要求する。RenderPassEndの後に呼び出す。DefaultPhase / AHO DrawPhaseにも使用できる。
- slot不足ならnullopt。その要求は受理されず、既存のTicketの結果を上書きしない。
- `info()` はworker内で一意なrequest_id、caller指定frame_id、正規化したregion（layer含む）、format、row_stride、byte_sizeを保持する。
- PENDING / SUBMITTED / COMPLETE / CANCELLEDを区別する。記録だけで未submitのwaitはfalse、readはlogic_error。記録をcancel / 例外で中断するとTicketもCANCELLEDとなり、readできない。
- Ticketのコピーは同じ結果を共有する。完了後も全コピーを破棄するまでslotを占有し、readを繰り返せる。回収後にoptionalをresetしてから次を要求する。
- Ticketはhelperやproducer managerのハンドルより長生きしても、完了したデータを読み取れる。VSLの送信Completionが転送先mapと元画像を保持する。CPUからの同じworker / queue / Completionへの並行アクセスは利用側で同期する。
- 空いたslotを使い回し、十分な容量のslotを優先する。足りない場合だけBufferを拡張する。capture_ticketの独立pool / buffer群 / Fenceはslotごとに一度用意し、毎回作り直さない。
- max_requestsは既定1、0は拒否する。initial_capacityは最初のslotの予約容量。capacityは確保済みslotのBuffer要求byte容量の合計で、Vulkan割当paddingは含まない。

既存captureは最新結果を内部保持する従来の便利APIであり、Ticket APIの結果は返されたTicketから読む。両方を同じworkerで切り替えるときはdiscard_latestで従来の最新結果を手放せる。進行中のGPU処理は取り消さず、そのslotは完了するまで再利用しない。

## AHO DrawPhaseからの接続

finish callbackでRenderPassを終了し、その後にrecordする。SourceとなるSwapchainにはTRANSFER_SRC usageが必要で、Surfaceの対応確認と画像数に合うpresent用Semaphoreの用意は事前に行う。

```cpp
std::optional<aho::ImageReadbackTicket> pending;
{
    aho::DrawPhase phase(engine, nullptr, [&](auto &stream) {
        stream << vsl::command::RenderPassEnd{};
        pending = reader.record(phase, chain.images.at(phase.get_image_index()),
                                vsl::ImageLayout::PRESENT, {x, y, 1, 1}, frame_id);
    });
    // 描画する。終了コールバックの後に、同じPhaseを送信して画面表示する。
}
```

この経路も既存のDrawPhaseを使った実GPUテストで検証している。画像エンコード、上下反転、色空間変換は今回のAPIに含めない。

finish callbackは自動送信だけでなく明示的なsubmit / submit_commandsでも、送信前に一度だけ実行する。基底Phaseの参照経由でも同じ動作とする。callbackを指定しなければ既定RenderPassを終了する。

finish callbackから例外が出た場合は記録をcancelして取得画像を返す。明示送信では例外を呼び出し元へ伝播し、デストラクターでは外へ投げずログへ通知する。初期化callbackの例外はthread-local状態を復元して伝播する。フレーム本体の例外巻き戻しではfinish callbackを実行せずcancelする。通常のネスト時のthread-local復元を再設計した変更ではない。

## 2026-10-19 検証結果

Apple M1 / MoltenVK、Vulkan SDK 1.4.309.0、Debug。VSL全体とAHOのReadback / DrawPhase、公開ヘッダーとVulkan直接依存禁止のcompile契約で227件成功、失敗0、環境skip 0。既存26件はDISABLEDを維持する。新規のGPUケースはvalidation Layer付き。Windows / Linux実行、AHO全CPU suite全体の成功を示す結果ではない。

新規検証は、キー指定の不足・重複・型・範囲、同一Phase内のID/カラー転送、フレームと結果の対応、保持中の結果不変性、helper破棄後の読取り、cancel・例外後の再利用、24フレームのslot再利用、容量に合うslot選択、Timeline SemaphoreでGPUを停止した状態での2件受理と3件目拒否、AHO DrawPhaseのfinish callback接続とcallback例外・初期化例外・巻き戻し後の次フレーム実行を含む。修正前にはfinish callback例外によるプロセスabortを回帰テストで再現した。

Vulkan API呼び出しを別途計数した。

| ケース | Buffer作成 | pool作成 | CommandBuffer割当呼び出し | Fence作成 | map | submit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 描画 + ID + カラーの同一Phase | 2 | 1 | 1 | 1 | 2 | 1 |
| recordによる24フレーム | 1 | 1 | 1 | 1 | 1 | 24 |

submitの追加や毎フレームのGPUリソース作成へ戻っていないことを確認した。要求ごとのTicket / Completion等のCPUメタデータ生成はあり、時間による性能比較は再計測していない。

完全なc++使用例はnative Vulkan識別子を禁止して構文compileし、triangle.vert / picking.fragを使う一時harnessからvalidation Layer付きで実GPU実行した。ID 42、取得frame_id、1280×720 RGBA8のサイズと期待画素を確認した。

```sh
ctest --test-dir build-vsl-tickets --output-on-failure \
  -R '^(VSL\.|AHO\.(ImageReadback(Busy)?VulkanIntegrationTest|DrawPhaseVulkanIntegration)\.|AHO\.Compile\.(Header\.image_readback\.hpp|ImageReadback\.PublicVSLOnly)$)'
```

## 2026-10-20 セルフレビュー

DrawPhaseで明示送信するとfinish callbackを通らず、RenderPassEndやその後のReadback記録を飛ばす問題を修正した。DefaultPhase::submit_commandsを仮想化し、DrawPhaseの送信前処理へ統一した。キャンセル用の空送信はこの前処理を通さず、捨てるフレームにfinish callbackを実行しない。

修正前に基底参照経由の明示submit / submit_commandsでcallbackが呼ばれないことを再現した。既定RenderPassの明示送信と、明示送信中のcallback例外を一度だけ通知して次フレームへ進むケースも追加した。VSL / AHO / 利用側は仮想関数の追加を含めて再ビルドする。

親の#105で修正したSemaphore保持と外部Fence再利用を取り込んだ構成で、関連232件成功、失敗・環境skip 0。既存26件DISABLEDは維持。DrawPhaseの10件はvalidation Layer付きで成功した。Windows / Linux実行、物理的なnon-coherent heapと全AHO CPU suiteはこの検証に含めていない。

## origin/master追従後の検証（2026-10-20）

`18c481a23005c2d2a09b328b5d414dfb30c50181`を基点に、#56 → #105 → #109 → #114の依存順を保ってrebaseした。format定数、CommandBuffer、同期処理、既存テスト、shader fixture一覧の競合を解消し、master側の回帰テストを保持した。Vulkan fixtureのvalidation Layer初期化も両方の指定方法を扱う形へ統合した。

| PR | 検証範囲 | 成功件数 |
| --- | --- | ---: |
| #56 | VSL全体、AHO読み戻し、公開ヘッダーcompile | 354 |
| #105 | AHO GPU全体、公開ヘッダーcompile | 25 |
| #109 | AHO GPU全体、公開ヘッダーと公開VSL依存境界compile | 26 |
| #114 | VSL全体、AHO GPU全体、公開ヘッダーと公開VSL依存境界compile | 398 |

いずれもM1 / MoltenVK・SDK 1.4.309.0・Debugで失敗・環境skip 0。最新masterに残るVSLのDISABLED 2件は維持した。AHO GPUはEngine / Window / pipeline / DrawPhase / Readbackを含み、#114で39件成功。Windows / Linux、物理的なnon-coherent heap、全AHO CPU suiteの検証結果ではない。master側のWindow仮想デストラクター追加を含め、各ライブラリと実行ファイルを同じヘッダーで再ビルドした。
