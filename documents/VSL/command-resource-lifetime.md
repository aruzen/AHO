# コマンド記録後のリソース寿命

ComputePhase/DefaultPhaseの記録中は、VSL標準コマンドが使用する所有者をSubmissionWorkへ登録します。Completionは[観測結果だけ](completion-ownership.md)を共有します。記録後に呼び出し側のラッパーを解放しても、GPU処理が終わるまでnativeオブジェクトを保持します。

対象は頂点・index buffer、GRAPHICS/ComputePipeline、BindPipeline、descriptorとその更新済みリソース、PushConstantのlayout、画像barrier、画像とbufferの転送、RenderPassBeginのframebufferです。ImageReadbackの既存の保持も継続します。

```cpp
vsl::Completion completion;
{
    auto phase = manager.start_phase<vsl::ComputePhase>();
    {
        auto vertices = make_vertices();
        phase << vsl::command::BindVertexBuffer(vertices);
        // 記録したbufferの所有権はPhase側にも残る。
    }
    completion = phase.completion();
}
completion.wait();
```

COMPLETION保持は完了／取消／失敗の確定時に回収します。従来のSUBMISSION保持はWork寿命まで残り、slot再利用やmanager破棄で回収します。Completionを保存してもWorkを保持しません。取消済みの結果だけで資源を延命することはできないため、必要ならowning viewを保存します。slot再利用時は前のGPU送信を待ってから置き換えます。

独自コマンドは`buffer.retain(owner)`をnative記録より先に呼んでください。Phaseを通さない手動記録ではretainは保持を追加しません。従来どおり呼び出し側でGPU完了まで所有してください。shared_ptrで渡したコマンド自体の保存は行いません。CommandManagerなどをコマンドが所有する場合の循環を避け、使用するリソースを明示的に保持します。

BindVertexBuffer/BindIndexBuffer等が借用するラッパーはinvokeまで必要です。記録後のラッパー解放に対応する変更であり、invoke前の借用先解放には対応しません。descriptorのupdateやPoolの明示destroy、bufferの書き換えに必要なGPU同期は呼び出し側の責任です。

BindVertexBufferAt／BindIndexBufferAtやTypedの所有するコマンドは生成時にstorageのownerを保存します。legacyのラッパー借用経路とは区別してください。

BindVertexBufferの空一覧とBindGraphicResourceの空vectorは、nativeコマンドを記録せず、既存のbindingを保持します。個数0のnative bindはVulkanで許可されないため、空の場合は呼び出しを省略します。

資源のretain自体はCPU上の保持操作です。DefaultPhaseでは別途acquire fenceによるsignal完了観測を追加しており、native生成・待機の残るコストは[完了管理の文書](completion-ownership.md)を参照してください。CPU処理時間のベンチマークは実施していません。
