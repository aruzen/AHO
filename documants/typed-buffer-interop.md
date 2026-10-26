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

## AHO adapter

- uniform_view<PayloadTag>／storage_view<PayloadTag>／vertex_view<Vertex>は既存のVSL Typed viewを返す。標準BufferとCustomに共通のgpu_buffer conceptを使い、CustomのStorage／Customizationを型消去して取り出す処理は追加しない。
- Storage::TYPEが分かる場合はusageをrequiresで検査する。binding()だけを提供する独自wrapperではnative view factoryによる実行時検査を行う。
- 範囲はcapacityではなくAHOの論理sizeから求める。first/countはレコード単位で、空・範囲外・桁あふれを拒否する。count省略時は端数byteも拒否する。native handleやサイズの変更後はview／descriptorを作り直す。
- typed_texture(texture, sampler)はTextureの使用状態をTyped descriptorへ引き継ぐ。descriptor配列、draw中のupdate拒否、完了後の再更新も同じ経路で扱う。
- index viewは任意byte列から生成しない。既存TypedIndexBufferのCPU snapshotによるindex範囲検証を維持する。
- ウィンドウ付きTyped sampleをAHO Bufferによる頂点／各FrameのUBOへ切り替える。型宣言・push constant・index・batch・slot再利用は既存Typed APIを使う。

placement new／allocatorやschemaからの一括メモリ計算はこのstackの対象外。まず型を保った接続と寿命／更新の共通規則を用意する。

## 実装・検証メモ

環境の空き容量確保のため、以前の自分の2つの検証ビルドから再生成できる.o中間ファイルだけを削除した。今回のビルドはLLVM 20 / C++26、-O1 -g0 -DDEBUG、並列数1で行う。

型付きStateの変更後、既存のBuffer／Custom／Texture GPUテスト19件成功。変更C++ 3ファイル／4TUのformat・命名・配置検査は違反0件。

stack全体の最終検証は以下の通り。MoltenVK 1.4.2とvalidation layerを有効化した。

- VSL Typed／ResourceUse／TrackedDraw／Phase関連185件成功。うちcompile contractは80件。複数Phase、descriptorの遅延参照、batchの省略bind、Frameの循環参照防止、送信破棄後の完了／cancel観測を含む。
- AHO Buffer／upload／TextureUpdate／Typed adapterのGPUテスト26件成功。うちadapterは6件。実際のcompute出力、device-local頂点のindexed draw、Texture配列の画素、Custom所有者の寿命を検証した。
- AHO adapterの正負compile contractと単独header includeの10件成功。
- 共通使用状態の変更C++ 28ファイル／18TU、adapterとSampleの6ファイル／3TUでformat・命名・配置の違反0件。
- Typed Sampleの--frames 120成功。--verifyも成功し、3 slotsを4周する12送信、左右2描画、UBOの時間変化238831画素、resize 1720x1080→2000x1360、pipeline生成1回を確認した。今回の実行でVulkan validation errorはない。
