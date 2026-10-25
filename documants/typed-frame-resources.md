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

## 今回の範囲外

型宣言全体からのメモリ計算・arena・placement newは今後の段階。GPU資源の配置とホスト側オブジェクトの配置は別途設計する。
