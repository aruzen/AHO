# 歩行サンプルから決めるAHO/VSL実装順序

目的は、参考画像のような世界を歩けるサンプルを実際に動かし、エンジン機能の不足と依存順を確認すること。作業先は `make-sample-game`。以下の「対応済み」は今回の散策サンプルで確認した範囲を指し、汎用エンジンとして完成したという意味ではない。

## 実装した順序と責務

| 順序 | 必要な機能／発見した不足 | 対応 |
|---|---|---|
| 0 | 新規worktreeで再現できる依存準備 | stbの相対include先が不統一でヘッダーもgit管理外だったため、CMakeでcommitとSHA256を固定して取得。glTF PRのtinygltf取得を利用。macOS runtimeも後述の固定版へ揃える。 |
| 1 | window、GPU command、同期、color/depth、readback | 既存AHO `StandardEngine` / `Window` / `DrawPhase` / `x::Screenshot` とVSL公開APIを使用。resize時はGPU完了後にattachment/framebufferを再構築し、形式が互換ならdynamic viewport/scissorのpipelineを保持する。 |
| 2 | 座標規約が明確なcamera | AHOの既存行列型を使う `x::scene::Camera` を追加。右手系・Y-up・Vulkan depth [0,1]・射影Y反転を明示。既存 `make_view` の180度rollやOpenGL型の射影の契約は変更しない。 |
| 3 | CPUメッシュから深度付き3D描画への入口 | `x::scene::MeshRenderer` を追加。頂点buffer、depth、render pass、framebuffer、pipelineを所有し、DrawPhaseに描画を記録する。shaderは利用側が渡す。 |
| 4 | glTF asset→GPU texture/mesh→骨格アニメーション | PR #201/#206を取り込み。#201の `GltfModel::load/sample/skinMatrices` を利用し、#206のviewer内GPU実装を `x::scene::GltfRenderer` へ切り出した。地形と同じdepth passへFoxを配置。関節palette/descriptorはframe slotごとに所有し、そのslotの完了後に更新する。 |
| 5 | 一人称入力とwindow focus | 既存 `InputManager` / `Key` / `MouseButton` / `Cursor` を使用。Cursorにcapture、focus確認、捕捉切り替え・focus復帰時のdelta抑止を追加。Windowにshow/focusを追加。サンプルからGLFWを直接呼ばない。 |
| 6 | 地形の高さと歩行・衝突 | 描画と同じ三角形を補間する高さ取得、幹・岩・岸・外周への衝突、axis slide、移動substepをサンプルに実装。`core/collision.hpp`の空のCollisionJudgerを汎用物理完成扱いにはしない。 |
| 7 | 世界の見た目と実画像による検証 | 地形・森林・雪山・湖を決定的に生成。昼夜、霧、反射を近似する水面、オーロラ、雪粒子、操作表示をサンプルshaderに実装。CPU検証とGPU画像検証を用意。 |

描画経路は `World / GltfModel → MeshRenderer / GltfRenderer → DrawPhase → VSL → swapchain`。入力は `InputManager → Walker → Camera`。GPU画像の確認には同じDrawPhaseへ `Screenshot` を接続する。ゲーム側でも描画器側でも、生のVulkanやVSLの `_pimpls.hpp` を使わない。

## 詰まった点と最終対応

### resize後に全画素が同じ色になる

SDK 1.4.309同梱のMoltenVK **1.2.11**で再現した。初期描画、歩行、異なる時刻、frame slot再利用後の同時刻画像一致までは成功し、resize後だけ画像が単色になった。

深度無効化・Store変更で一度通ったが、拡大・再縮小を加えると再現した。event処理、互換pipelineの保持、明示depth clearでも解消せず、GPU上のcamera行列は正常だった。画面座標を色にする診断shaderでも先頭画素が全面に広がったため、深度原因という初期仮説を棄却した。

[MoltenVK v1.4.2の公式release](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2)に、swapchain再作成後の1×1 drawable修正が記載されている。実際に**1.4.2へ切り替えると、通常の深度比較・DontCare storeで縮小、拡大、再縮小がすべて成功**した。旧driverの現象と修正版での結果から、この不具合に該当すると判断した。

`sample-game/cmake/MoltenVK.cmake`で公式配布物をSHA256固定でbuild以下へ取得する。サンプルのプロセスとGPU CTestだけでこのdriverを選び、既存SDKやシステムは変更しない。オフラインICD指定、system driverへの切り替え方法は `sample-game/README.md` に記載。診断用shader、待機、比較Always、明示clearの試作は最終コードから除去した。

**実装順序への反映:** GPU実装を評価する前に、loader/driverの版も再現条件へ含める。また、既存glTF viewerのresize検証は画素寸法だけを検査するため、同じ欠落を見逃せた。歩行サンプルはresize先頭フレームにも色数・画素内容の検査を行う。

### 例外時のnative window終了順

保存先エラー等で通常の `window.close()` へ到達しないと、終了時に異常が出た。ゲームにclose guardを追加し、renderer破棄後・engine破棄前にwindowを閉じる。保存先を既存実行ファイルにした負例テストは、abortせず終了コード1になることを確認する。

### 雪山のちらつき

雪冠と岩肌のconeを重ねる方式では遠距離の重複面にz-fightingが発生した。共有稜線で岩肌と雪の面を分割し、重複面をなくした。エンジンのdepth biasで覆い隠す対応にはしない。

## 次に実装する順序

| 優先順 | 機能 | このサンプルで得た根拠 |
|---|---|---|
| 1 | 独立したRenderTargetとScene/Transform/instance管理 | 現状はMeshRendererがtargetも所有し、GltfRendererがそれへ依存する。まずtargetを独立させ、assetと配置・変換を分離する。これはviewer、複数camera、postprocessでも共通になる。 |
| 2 | 3Dの地形queryとcharacter collision契約 | 高さ取得、境界、circle collider、slideがサンプルに閉じている。地形queryと動く物体の位置更新を分離し、sweep、段差、斜面、capsuleへ進む。今回の高さ追従は汎用物理・重力・ジャンプではない。 |
| 3 | GPU mesh/textureの共有、instancing、frustum culling | 環境は149,708三角形、2,030 collider。木をすべて展開した静的bufferで約18 MBを使い、view外も送信する。glTF rendererも1 renderer/1 instanceでGPU asset共有はない。計測してこの重複を減らす。 |
| 4 | 共通environment/material定義とshader入力契約 | 地形とglTF shaderに照明・霧・tone responseが重複する。共通のframe uniformとmaterial表現を先に作り、その後PBR、sampler各設定、mipmap、透過sortへ広げる。現在は不透明triangle、4 influence、base color textureの範囲。 |
| 5 | shadow map、HDR、bloom等のpass構成 | 木の接地影は地形上の薄いmesh、水面は空の近似反射。動的な実影、実景の水面反射、HDR/postprocessは未実装。独立RenderTargetとpass構成が先に必要。 |
| 6 | 2D overlay、font、UI、音声 | 今回のHUDはshader内の簡易glyph。#202〜#204のDrawList/Renderer2Dは確認したが今回取り込んでいない。3D pass上のoverlayとfont APIへ接続する段階で使う。#151は手動VertexInputを使う今回の経路には必須ではない。 |

今回実際に必要になった境界を、この順で分離する。`x/scene` はその契約を確かめる実験APIとする。

## 検証結果

2026-10-22、Apple M1 / macOS、Homebrew LLVM、Release、MoltenVK 1.4.2、Khronos validation有効で確認。

- 関連CTest **72/72成功**（CPU/API/header compileと実window GPU 3件を含む）。追加・既存の入力テストは別途 **41/41成功**。
- world CPU検証: 1800ステップ、岸、外周、2030 collider、頂点・法線の有限性。camera 4件: Vulkan near/far、上下左右、移動・回転、無効入力。
- GPU: 湖畔・雪の森・オーロラ、移動による画素変化、昼夜、時刻変化、slot再利用後の同時刻画像の完全一致、resize先頭フレームと拡大・再縮小を検証。Vulkan validation errorは0。
- 2560×1512の湖畔は2826 sampled colors、雪の森1201、夜の森10426。resize後1920×1200は9717、2200×1400は10172で、全画素単色の回帰を検出できる。
- 取り込んだglTF viewerも実windowでRunの異時刻差分、slot再利用、resizeを検証。モデルforeground112951画素、時刻間変化140979画素、1453色。
- 保存先エラーはfilesystem errorを出して終了コード1になり、abortしない。MSBuild XMLとshell構文、diff checkも確認。
- 描画PNGは `build/wander/sample-game/captures/`、起動・検証方法は `sample-game/README.md`。lake/snow/aurora/HUDの画像を目視確認した。
- `./sample-game/run.sh --view 3 --frames 180` も正常終了。通常ループ180 frameは1.52秒（約118 fps、validation有効、2560×1512）。この環境での短い実測であり、最適化判断の初期値とする。

Windows/Linux、全AHO/VSLテスト、sanitizer、PBR/汎用物理は今回の検証範囲に含めない。
