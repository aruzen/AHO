# Typed APIの所有・宣言・shader契約の整理

PR #221の後続。次の3点を順に解決する。

1. `TypedVertexBuffer<Binding>` がbinding番号・location・入力頻度まで所有型へ結び付けており、同じGPUデータを別pipelineへ再利用しにくい。bufferをレコード型へ結び、接続情報はpipelineのbindingへ分離する。
2. C++レコードとformat/offsetの対応をbindingごとに繰り返す。レコードlayoutを一度定義し、binding宣言と検査用metadataをそこから導出する。C++のpaddingを尊重し、正規化整数のようなformatの意図も明示できるようにする。
3. Typedの静的検査だけでは、ロードしたSPIR-Vとの一致を確認できない。実際にpipelineへ注入されたvertex shaderをreflectionで照合し、不一致をpipeline作成前に診断する。C++側のコンパイル時保証、shader側の実行時保証、device検証の境界を記す。

向け先は統合済み #221 の `typed-pipeline-base`。実装・検証結果は各段階で追記する。

## 1. bufferとbindingの分離

`TypedVertexBuffer<Vertex>` はレコード型だけを保持する。`VertexBinding<Vertex,...>` と `TypedVertexBinding<Input>` がpipelineへの接続を受け持つ。同じレコードのbufferはbinding番号・location・入力頻度の違うpipelineへそのまま接続でき、再uploadやbackendへの型消去は不要。bufferの引数順・数・レコード型は引き続きコンパイル時に検査する。

既存Typedテストとcompile contractを新しいAPIへ移行。同じallocationを通常bindingと疎なbindingのpipelineへ渡し、両方のGPU readbackを確認する。

## 2. レコードlayoutを一度定義する

`VertexRecord<Vertex, Members...>` を導入。`VSL_VERTEX_MEMBER(Vertex, member, location?, format?)` はメンバー型とoffsetofを一緒に渡し、通常のscalar・C配列・std::arrayのformatを推論する。既存の `GRAPHIC_TYPE` を持つ型にも接続する。正規化整数等はformatを明示できる。

`Record::Binding<binding, timing, first_location>` は共通layoutからbindingを生成する。locationはレコード内の相対値を一度解決し、first_locationを加算する。strideはsizeof(Vertex)、offsetはoffsetofを使う。異なるrecordのmember、memberのstorageを超えるformat、未対応型、location overflowはコンパイル時に拒否する。明示formatの意味上の解釈は呼び出し側が選ぶ。

C++23ではmember pointerからportableにconstexpr offsetofを求められないため、マクロは型とoffsetの取得だけに限定した。layout生成と検査はテンプレートで行う。padding付き構造体、float/正規化整数の混在、std::array<double,3>の2location消費を検証する。
