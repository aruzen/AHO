# コマンド記録後のリソース寿命

ComputePhase/DefaultPhaseの記録中は、VSL標準コマンドが使用する所有者をCompletionへ登録します。記録後に呼び出し側のラッパーを解放しても、GPU処理が終わるまでnativeオブジェクトを保持します。

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

所有者はCompletionとframe slotの参照がなくなった時点で回収されます。wait()/ready()だけでは保持を解きません。slot再利用時は前のGPU送信を待ってから置き換えるため、古いCompletionを保存しなければ継続描画でも保持が増え続けることはありません。キャンセルした記録の所有者は、そのCompletionが解放されるまで残ります。

独自コマンドは`buffer.retain(owner)`をnative記録より先に呼んでください。Phaseを通さない手動記録ではretainは保持を追加しません。従来どおり呼び出し側でGPU完了まで所有してください。shared_ptrで渡したコマンド自体の保存は行いません。CommandManagerなどをコマンドが所有する場合の循環を避け、使用するリソースを明示的に保持します。

BindVertexBuffer/BindIndexBuffer等が借用するラッパーはinvokeまで必要です。記録後のラッパー解放に対応する変更であり、invoke前の借用先解放には対応しません。descriptorのupdateやPoolの明示destroy、bufferの書き換えに必要なGPU同期は呼び出し側の責任です。

追加する処理はCPU上のshared_ptr保持です。GPUコマンド、送信回数、完了待ち、nativeリソースの生成回数は増やしません。CPU処理時間のベンチマークは実施していません。
