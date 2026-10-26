# Typed Sampleで確認した範囲

`AHOLi/AHO/examples/typed`の`aho-typed-example`は、頂点、descriptor、push、index、view、batchと、フレーム単位のTyped資源を組み合わせるウィンドウ付きSample。

## 実装と同期

型宣言はscene.hpp、描画資源と記録はscene.cppへまとめた。型からnative layoutを作り、GLSLの実際の入力とblock配置はpipeline生成時に照合する。

`ResourceAllocationSchema`へフレームごとのScene UBOと共有Palette UBOを宣言する。`TypedResourceBundle`が要素型・usage・bindingからbufferとdescriptorを生成し、接続する。手書きの`SceneFrame` constructorとwrite接続を削除し、`SceneFrame`は生成されたFrame型のaliasにした。

Paletteは`initialize_shared`で一度初期化する。毎フレーム、色・カメラ位置・aspect比・時刻を`resources.update(phase, callback)`で現在の枠へ書き込み、`resources.get(phase)`から`frame.bind_resources(...)`で両setを描画へ渡す。内部のTypedFrameResourcesと既存Phaseが枠の前回送信の完了を待つため、使用中のUBOを更新しない。

標準3枠ではbuffer 4個、descriptor set 4個、descriptor 4個、論理payload 112 bytesを計画する。Sceneの3個とPaletteの1個で、全bindingが共有のPalette setは1個を再利用する。実際のGPU allocation sizeはdriverのrequirementsによって別途決まる。

quadごとの位置・scale・回転速度はpush snapshotで渡す。pushの値自体は時間で変えず、アニメーションはUBOの時刻を使用する。画像の変化だけでpushとUBOの効果が混同されないようにした。vertex/indexは構築後に変更しない。

device・descriptor manager・render pass・format・viewport/scissorは公開`RenderContext`から取得する。Sampleから`data_`／`data2`への直接アクセスを除いた。保持したcontextはresize後の描画先に追従する。

swapchain/標準pass/framebufferはAHOのWindowに任せる。GPU完了後のWindowResizeHookPluginでattachment形式を確認し、互換ならTyped pipelineを保持する。viewport/scissorは毎フレーム動的に設定し、形式が変わればpipelineを再生成する。標準windowの1色attachment・subpass 0を使う。

## 今回追加して解消した不足

- エンジン／window内部を辿って描画に必要なサービスを取得していた部分を、公開描画コンテキストへまとめた。
- フレーム枠ごとのbuffer/setの作成・選択・所有をTypedFrameResourcesへまとめた。
- staticなUBOでは確認できなかった更新と再利用を、動くカメラと色のUBOで確認できるようにした。
- 型付き資源のconstructorへ残っていたbuffer／set生成とwrite接続を、割当schemaからの自動生成へ移した。

型付き資源の所有、callbackとaliasの契約は[typed-frame-resources.md](typed-frame-resources.md)、割当宣言と自動生成は[typed-resource-allocation.md](typed-resource-allocation.md)を参照。

入力イベント、起動、swapchain、画像保存は動的APIを使用する。一括arena確保、CPU object配置とGPU実メモリ要件を含む計画は今後の課題。

## 画像検証

`--verify DIRECTORY`は最初にフレーム枠の4周分を画像読み戻し待ちなしで送信し、その後、通常・UBO変更後・resize後の3枚を保存する。左右それぞれのquad、UBOによる画素変化、resize後の画像サイズ、互換形式でのpipeline再利用を検査する。

LLVM 20 / C++26 / Debug、MoltenVK 1.4.2 + validation layerで成功。3枠／12送信、238831画素の時間変化、1720×1080 → 2000×1360へのresize、pipeline生成1回、2drawのpipeline/vertex/index bind各1回、descriptor bind 2回を確認した。validation errorなし。変更C++ 2ファイル／2TUのstyle検査は違反0件。

追加のdriver指定なしでも通常イベントループの`--frames 120`が成功し、validation errorなし。

## ビルド時の注意と既知の構成

git管理外の`thirdparty/stb_image.h` / `stb_image_write.h`が別worktreeに無い場合は、既存の`thirdparty/setup.md`に従って配置する。SampleのCMakeでも不足をconfigure時に案内する。

初回Sampleのstyle CIでは`AHO_BUILD_TYPED_EXAMPLE=ON`が無く、main.cpp / scene.cppのコンパイルDB未登録とscene.hppの未解析で失敗した。workflowにもSampleの有効化を追加済み。今回も有効構成のDBでSampleの両TUとヘッダーを検査する。

通常ビルドは`BUILD_TESTING=OFF`で使用でき、SampleにGTestは不要。C++26、macOSの検証はLLVM 20/libc++で行う。Windows/Linuxは未実行。
