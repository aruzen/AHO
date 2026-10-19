# Phase・Readback基盤のmaster統合

## 統合の目的

PR #105・#109・#114はマージ済みだが、それぞれのマージ先が依存ブランチだったため、masterには変更が届いていなかった。スクリーンショット機能の前提となる既存の基盤を、masterへまとめて反映する。

| 既存PR | マージ先 | 統合する内容 |
| --- | --- | --- |
| #105 | `feat/vsl-render-pass-composition` | 取得画像番号とフレームスロットの分離、送信・同期・寿命管理 |
| #109 | `fix/vsl-phase-lifecycle` | 常時マップしたバッファと送信APIの公開、AHOからVulkan直接操作を除去 |
| #114 | `refactor/vsl-readback-boundary` | AttachmentIdによる指定、既存Phaseへ記録できるReadback Ticket、DrawPhaseの終了処理 |

基点はmasterの`0555815159886d6804f4f052ef2dc2c681764a04`。既存3本の最終実装は`43413b0abf393533067b974ddb82357e3bdc8589`を参照し、#56の最終状態との差分44ファイルを反映した。

対象ファイルについて、master側に追加されていた変更が名前空間表記・整形・コメントのみであることを確認した。`AHO_NAMESPACE`と`VSL_NAMESPACE`の使用、および現在の`.clang-format`に合わせたうえで、コードの要素が既存の最終実装と一致することを比較した。experimental機能の`x/`構成はmasterの状態を維持する。

スクリーンショットのAPIやPNG保存処理は、この統合には含めない。

## 関連文書

- [Phaseの取得画像と送信・同期](phase-lifecycle.md)
- [公開VSL APIと読み戻し](readback-boundary.md)
- [AttachmentIdとReadback Ticket](readback-tickets.md)
- [読み戻しの使用例](examples/readback-tickets.cpp)
