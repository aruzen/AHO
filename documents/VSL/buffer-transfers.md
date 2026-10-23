# バッファ間転送

`destination.copy_by_buffer(manager, &source)`は送信元バッファの要求サイズ全体を転送先の先頭へコピーする。転送先が小さい場合は、コマンドを記録する前に`std::out_of_range`を送出する。コピー量を小さく切り詰める処理は行わない。

`LocalBufferHolder::copy_by_buffer`はholderのoffsetから転送し、holderのsizeを上限とする。マッピング範囲外の領域は書き換えない。

送信元には`TRANSFER_SOURCE`、転送先には`TRANSFER_DESTINATION`が必要で、双方とCommandManagerは同じデバイスに属する必要がある。空の送信元、無効なバッファ、同一ストレージへの重複コピーは`std::invalid_argument`で拒否する。

ここで行うのは保持済みの容量・用途・デバイス情報の比較だけで、データ走査やGPU問い合わせを伴わない。CPU側の`copy`と`uncheck_copy`の使い分けは従来どおりである。転送の完了待ちは従来どおり同期的に行い、後続のCPU読み出しに必要な可視性の確保は呼び出し側の責任とする。
