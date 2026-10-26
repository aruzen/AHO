# AHO BufferとTypedの接続

## 発見した不足

#287のAHO BufferはStorage／Customizationをtemplateで表すが、内部のResource所有者はshared_ptr<void>とstatic_castで取り出していた。また、使用状態の登録はupload／Texture更新に限定され、Typed draw／dispatchはAHOのbusyへ反映されない。BufferBindingはbyte単位で、既存Typed Buffer/Viewへ接続する経路もない。

このstackでは型付きState、共通の資源使用登録とVSL view factory、AHOのTyped adapterを順に追加する。基点は#287と公開時に取得したmasterの両方を含むtyped-buffer-stack-base。

## 型付きState

- TypedBufferState<Storage, Customization>が正確なBufferResource型のshared_ptrを所有し、GPUBufferBaseはその型から直接資源を取得する。
- BufferBindingの共通Stateは転送に必要なメタデータとweakな所有者参照を持つ。型を消して保持するのは転送用の参照と、GPU完了までのpin用スナップショット。
- stagingもGPU_FRIENDLY Bufferとして保持し、共通Stateから型付きStateへ戻すconstructorを削除する。
- CompletionにはStorage／Customization／mappingの所有者だけを渡す。共通Stateは送信をweak参照し、TypedFrameResourcesがFrame全体を保持しても循環参照を作らない。

## 共通の使用状態

- VSLのResourceUseStateへ記録中／送信済みの使用を登録する。複数Phaseでの使用を追跡し、すべての完了／cancel後にCPU更新を許可する。未送信Phaseが残るwait()は待たずにfalseを返す。
- AHO BufferUpload／TextureUpdateは別Phaseの使用との競合を拒否する。Typed draw／indexed draw／dispatchはvertex、index、descriptorが参照する資源を登録する。GPU間のbarrierやqueue同期を自動生成する機能ではない。
- Typed BufferのcopyとTypedDescriptorSetのwriteも使用中の更新を拒否する。descriptorの参照一覧はbind_resources()生成時ではなく記録時に読む。記録前のwriteは反映し、記録後のwriteは拒否する。
- TypedBatchが同一native handleのbindを省略しても、各コマンドの使用状態と所有者を登録する。
- VSLのTypedVertexBufferView／TypedResourceBufferViewにFromBufferを追加する。usage、型のalignment、実容量、非空範囲を確認し、native所有者と任意のResourceUseを保持する。
- CPU側の記録・更新は同一スレッドで行う。native()/backend()からの更新や取得済みmappingへの書き込みは追跡対象外。GPU間の書き込み競合はアプリ側で同期する。

descriptorを保持するgroupと資源使用stateは、送信を所有しない。送信がFrameやdescriptorの資源を保持しても、送信へ戻る強参照を作らない。

Textureの初期化済みlayout判定もCompletionObservationへ移す。送信を保持せず、生存中は現在の状態、破棄後は確定した完了／cancelを参照する。単純にweak_ptrだけにすると、送信の破棄後に完了とcancelを区別できず、次の部分更新で既存画像を消してしまう。結果の保存領域はobserverを作った送信だけに遅延生成する。

## 実装・検証メモ

環境の空き容量確保のため、以前の自分の2つの検証ビルドから再生成できる.o中間ファイルだけを削除した。今回のビルドはLLVM 20 / C++26、-O1 -g0 -DDEBUG、並列数1で行う。

型付きStateの変更後、既存のBuffer／Custom／Texture GPUテスト19件成功。変更C++ 3ファイル／4TUのformat・命名・配置検査は違反0件。
