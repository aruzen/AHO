# Typed Sampleで確認した範囲

`AHOLi/AHO/examples/typed`の`aho-typed-example`は、頂点、descriptor、push、index、view、batchと、フレーム単位のTyped資源を組み合わせるウィンドウ付きSample。

## 実装と同期

型宣言はscene.hpp、描画資源と記録はscene.cppへまとめた。型からnative layoutを作り、GLSLの実際の入力とblock配置はpipeline生成時に照合する。

`SceneFrame`にTypedUniformBufferとTypedDescriptorSetをまとめ、`TypedFrameResources<SceneFrame>`のFactoryがcommand buffer slotごとに別々のbuffer/setを作る。毎フレーム、色・カメラ位置・aspect比・時刻を`frames.update(phase, callback)`で書き込み、`frames.get(phase)`のconst参照からsetを描画へ渡す。既存Phaseが枠の前回送信の完了を待つため、使用中のUBOを更新しない。

quadごとの位置・scale・回転速度はpush snapshotで渡す。pushの値自体は時間で変えず、アニメーションはUBOの時刻を使用する。画像の変化だけでpushとUBOの効果が混同されないようにした。vertex/indexは構築後に変更しない。

device・descriptor manager・render pass・format・viewport/scissorは公開`RenderContext`から取得する。Sampleから`data_`／`data2`への直接アクセスを除いた。保持したcontextはresize後の描画先に追従する。

swapchain/標準pass/framebufferはAHOのWindowに任せる。GPU完了後のWindowResizeHookPluginでattachment形式を確認し、互換ならTyped pipelineを保持する。viewport/scissorは毎フレーム動的に設定し、形式が変わればpipelineを再生成する。標準windowの1色attachment・subpass 0を使う。

## 今回追加して解消した不足

- エンジン／window内部を辿って描画に必要なサービスを取得していた部分を、公開描画コンテキストへまとめた。
- フレーム枠ごとのbuffer/setの作成・選択・所有をTypedFrameResourcesへまとめた。
- staticなUBOでは確認できなかった更新と再利用を、動くカメラと色のUBOで確認できるようにした。

型付き資源の所有、callbackとaliasの契約、今後の割当schemaについては[typed-frame-resources.md](typed-frame-resources.md)を参照。

入力イベント、起動、swapchain、画像保存は動的APIを使用する。一括arena確保、型宣言からの割当サイズの自動計算は今後の課題。

## 画像検証

`--verify DIRECTORY`は最初にフレーム枠の4周分を画像読み戻し待ちなしで送信し、その後、通常・UBO変更後・resize後の3枚を保存する。左右それぞれのquad、UBOによる画素変化、resize後の画像サイズ、互換形式でのpipeline再利用を検査する。

LLVM 20 / C++26 / Debug、MoltenVK 1.4.2 + validation layerで成功。3枠／12送信、238831画素の時間変化、1720×1080 → 2000×1360へのresize、pipeline生成1回、2drawのpipeline/vertex/index/descriptor bind各1回を確認した。validation errorなし。変更C++ 3ファイル／2TUのstyle検査は違反0件、2shaderのspirv-valも成功。

追加のdriver指定なしでも通常イベントループの`--frames 120`が成功し、validation errorなし。

## ビルド時の注意と既知の構成

git管理外の`thirdparty/stb_image.h` / `stb_image_write.h`が別worktreeに無い場合は、既存の`thirdparty/setup.md`に従って配置する。SampleのCMakeでも不足をconfigure時に案内する。

初回Sampleのstyle CIでは`AHO_BUILD_TYPED_EXAMPLE=ON`が無く、main.cpp / scene.cppのコンパイルDB未登録とscene.hppの未解析で失敗した。workflowにもSampleの有効化を追加済み。今回も有効構成のDBでSampleの両TUとヘッダーを検査する。

通常ビルドは`BUILD_TESTING=OFF`で使用でき、SampleにGTestは不要。C++26、macOSの検証はLLVM 20/libc++で行う。Windows/Linuxは未実行。
