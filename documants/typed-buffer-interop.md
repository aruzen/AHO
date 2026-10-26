# AHO BufferとTypedの接続

## 発見した不足

#287のAHO BufferはStorage／Customizationをtemplateで表すが、内部のResource所有者はshared_ptr<void>とstatic_castで取り出していた。また、使用状態の登録はupload／Texture更新に限定され、Typed draw／dispatchはAHOのbusyへ反映されない。BufferBindingはbyte単位で、既存Typed Buffer/Viewへ接続する経路もない。

このstackでは型付きState、共通の資源使用登録とVSL view factory、AHOのTyped adapterを順に追加する。基点は#287と公開時に取得したmasterの両方を含むtyped-buffer-stack-base。

## 型付きState

- TypedBufferState<Storage, Customization>が正確なBufferResource型のshared_ptrを所有し、GPUBufferBaseはその型から直接資源を取得する。
- BufferBindingの共通Stateは転送に必要なメタデータとweakな所有者参照を持つ。型を消して保持するのは転送用の参照と、GPU完了までのpin用スナップショット。
- stagingもGPU_FRIENDLY Bufferとして保持し、共通Stateから型付きStateへ戻すconstructorを削除する。
- CompletionにはStorage／Customization／mappingの所有者だけを渡し、Completionを持つState自体は渡さない。

## 実装・検証メモ

環境の空き容量確保のため、以前の自分の2つの検証ビルドから再生成できる.o中間ファイルだけを削除した。今回のビルドはLLVM 20 / C++26、-O1 -g0 -DDEBUG、並列数1で行う。

型付きStateの変更後、既存のBuffer／Custom／Texture GPUテスト19件成功。変更C++ 3ファイル／4TUのformat・命名・配置検査は違反0件。
