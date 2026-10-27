# Typed frame resources

## 発見した不足と実装方針

- Typed Sample はdevice・descriptor allocator・render targetの取得に`engine.data_`／`window.data2`を直接参照していた。
- 毎フレーム変更するUBO／descriptorの所有と、GPUが使用中のメモリを更新しないための管理が利用側に残っている。
- `ComputePhase::prepare()`はcommand bufferのframe slotを再利用する前に前回の送信完了を待っている。この既存の同期を利用する。

このstackでは、AHOの公開描画コンテキスト、VSLの型付きフレーム資源、Sampleへの適用を順に追加する。

## 公開描画コンテキスト

`window.render_context()`または`phase.context()`で`aho::RenderContext`を取得する。

- `device()`、`descriptor_manager()`、`commands()`、`frame_count()`で作成・送信に必要なサービスを取得する。
- `render_pass()`、`format()`、`viewport()`、`scissor()`は呼び出し時の描画先を返す。保持済みコンテキストもresize後の描画先に追従する。
- コンテキストはエンジンと描画資源の所有者を保持する。取得したdescriptor managerの参照はコンテキストの寿命内で使用する。
- windowを閉じた後のアクセスは`std::logic_error`。返されたrender passなどの値は所有権を持つが、過去の値はresize後の描画先を表さない。
- window／phaseと同じスレッドで使用する。同期や描画の進行は既存のWindow／DrawPhaseが担当する。

## 検証

- 公開コンテキスト: LLVM 20 / C++26 / DebugでAHO・VSL・Typed Sampleをビルド。
- RenderContext・既存DrawPhase・WindowのGPUテスト45件と公開ヘッダー単独コンパイル1件、計46件成功。MoltenVK 1.4.2、validation errorなし、skipなし。
- 変更C++ 6ファイル／4TUのformat・命名・配置検査で違反0件。
- TypedFrameResources: GPUテスト7件・コンパイル契約7件成功。3枠／12送信でUBO値と計算結果、descriptorの独立性と再利用を確認。skip・validation errorなし。
- TypedFrameResourcesの変更C++ 4ファイル／3TUのformat・命名・配置検査で違反0件。
- 既存Typed各機能・Phase・新規frame資源の回帰テスト177件成功。公開コンテキスト側と合わせて計223件、skip・validation errorなし。
- Sampleの画像検証: 3枠／12送信後に読み戻し、238831画素のUBOによる時間変化、1720×1080 → 2000×1360へのresize、pipeline生成1回を確認。実画像でも左右のquadを確認。
- Sample変更C++ 3ファイル／2TUのformat・命名・配置検査で違反0件。Sampleの2shaderと新規compute fixtureのspirv-val成功。

## 型付きフレーム資源

`vsl::typed::TypedFrameResources<Frame>`は、利用側で定義した`Frame`をCommandManagerの各command buffer slotにつき1つ生成する。`Frame`にはTypedUniformBuffer・TypedStorageBuffer・TypedDescriptorSetなどをまとめる。

```cpp
vsl::typed::TypedFrameResources<Frame> frames(context.commands(), [&](size_t slot) {
    return Frame(context.device(), context.descriptor_manager());
});

aho::DrawPhase phase(engine);
frames.update(phase, [&](Frame& frame) {
    frame.uniform.copy(std::span(&parameters, 1));
});
const auto& frame = frames.get(phase);
phase << pipeline.bind_vertices(vertices).bind_resources(frame.set).draw(3);
```

- Factoryはslot番号を受け取り、正確に`Frame`型の値を返す。資源の形は独自の構造体で宣言できる。descriptorの配列・複数set・storage bufferの可変要素数も既存Typed資源を組み合わせて表現する。
- 枠の数はcommand buffer ringから取得する。swapchainの画像番号・画像数は使用しない。
- `update(phase, callback)`は各Phaseで最初のアクセス時だけ許可する。`get(phase)`後や二度目のupdateは拒否し、記録済みcommandが使うbufferの後書き換えを防ぐ。`get`はconst参照を返し、繰り返し使える。
- callbackが例外を投げた場合、そのPhaseでは以後get/updateを拒否する。Phaseをcancelして次のPhaseで再度更新する。再入も拒否する。
- 記録中かどうかは公開stateだけでなく、内部のrecording submissionとCompletionの一致でも照合する。別manager・交換されたbuffer ring・未setup・送信済み・cancel済み・不正なslotを拒否する。
- 再利用時のGPU完了待ちは既存Phaseのprepareが担当する。Typed層は完了済み／cancel済みを確認し、独自の待機やsubmitは行わない。
- 使用したFrameの所有者をCompletionへ渡す。コンテナが破棄・moveされても、送信が使用するFrameは送信完了まで保持される。
- Frame型を持つコンテナのcopyは禁止し、moveを許可する。APIは単一のrecordingスレッドで使用する。

### 所有と更新の契約

Factoryは各slotに独立した可変資源を作成する。同じUBOやdescriptorのコピーを全slotへ渡すと、C++の型が同じでもGPUメモリは共有されるため、この抽象化の同期保証から外れる。変更しない画像・samplerなどは共有できる。

update callbackの可変参照やbuffer/setの可変aliasを保存し、後から更新してはならない。callback内ではcommandの記録・submit・cancelを行わず、資源の更新に限る。getの参照と、それを使って作成したcommandは取得したPhase内で使用する。既存Typed APIのbackend escapeと同様、C++の外部aliasまで静的に封じるものではない。descriptorを差し替える場合も最初のupdate内で行う。Frameは資源の所有者をまとめ、管理元のCommandManagerやTypedFrameResourcesへの所有参照を含めない。

現段階ではResourceSchemaから全資源を自動生成しない。shaderのdescriptor schemaはbindingとpayload配置を表す一方、storage bufferの要素数や共有資源の配置は利用側が決める必要がある。これらの割当方針を型へ拡張する前段として、任意の型付き資源構成と安全な再利用の境界を導入する。

## Sampleへの適用

後続の割当宣言・自動生成の追加により、現在のSampleでは手書きのSceneFrame constructorを削除した。TypedFrameResourcesによる枠と所有の管理はTypedResourceBundleの内部で再利用する。現在の生成経路は[typed-resource-allocation.md](typed-resource-allocation.md)を参照。

Sampleの`SceneFrame`へUBO／descriptorをまとめ、全slotで独立して生成する。カメラ位置・色・aspect比・時刻を現在のslotへ書き、quadごとの位置・scale・回転速度はpush snapshotへ渡す。保持したRenderContextから現在のrender pass・viewport/scissorを取得する。

Sampleから内部の`data_`／`data2`参照を除いた。`--verify`ではフレーム枠の4周分を読み戻し待ちなしで送信してから画像を取得し、UBOだけの時刻変更で画素が変わることと、resize後の描画・互換pipelineの再利用を検査する。

## 今回の範囲外

型宣言全体からのメモリ計算・arena・placement newは今後の段階。GPU資源の配置とホスト側オブジェクトの配置は別途設計する。
