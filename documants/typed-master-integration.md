# Typed Buffer系列のmaster統合

## 対象

開始時のmaster 101cd55aを起点に、#288〜#290の型付きBufferState、共通使用状態、AHO Buffer／TextureのTyped adapterを取り込む。公開前に#293がmasterへmergeされたため、最新79a29193へrebaseした。割当schema／TypedResourceBundleを含む最終構成で検証する。

## masterとの調整

- glTF／MSDF系列が追加したCPU BufferからのTexture更新、sRGB対応、既存の転送範囲検証を保持する。
- Texture更新はCPU stagingの生成前に共通使用状態を確認する。Typed drawで使用中のTextureは、CPU／GPUどちらの入力からの別Phaseのupdateも拒否する。
- 検証にはmasterのCPU staging・sRGBテストと、Typed draw中のCPU Buffer更新拒否／完了後の再更新を含める。
- サンプル本体はmasterの割当bundle版を保つ。今回の接続APIはdocuments/AHO/resource-buffer.mdと実GPUテストで示す。以前の#290のサンプル適用・検証結果は履歴として残す。
- master向け差分と#293のmerge-treeを確認し、両方のAPI・所有の規則が整合することを検証する。

## 検証方針

Clang 20 / C++26、-O1 -g0、ASan／UBSanで製品とテストをビルドする。既存の同一compilerで構築された依存ライブラリと固定済みsaitekikuukan／tinygltfを再利用し、検証用buildはこのworktreeの1個にまとめる。

rootのmaster checkoutとAHOLi/CMakeLists.txtの手元変更、既存#293のbranch／worktreeには変更しない。

## 併用と公開前の更新

- 最初のmaster向け構成ではCPU1068件、AHO GPU106件、VSL657件成功／非対応format3件skip、compile契約95件、変更C++33ファイル／21TUのstrict style違反0件を確認した。
- #293とのmerge-treeは無競合。今回のbackendに対してAllocation／BundleのCPU5件・GPU9件とAHO外部Uniformの接続1件、compile契約26件が成功した。
- 応答待ちになったSSHのfetchを終了し、BatchMode／ConnectTimeout／ServerAliveを指定して接続を張り直した。取得したmaster79a29193へ無競合でrebaseした。rebase後のtreeは、先に検証した併用treeと同一だった。
- 外部AHO Uniform→TypedResourceBundle→dispatchでbusyへ反映され、CPU更新を拒否し、完了後に出力41を読み戻して更新を再開できる検証を、AHO GPUテストへ追加した。

## 最終検証結果

- 最新master79a29193（#293反映済み）上で、通常AHO・VSL・CPU/GPUテスト・Typedサンプルを再ビルドした。
- Clang20 / C++26 / -O1 -g0、ASan／UBSan、MoltenVK1.4.2とvalidation layerで、AHO CPU1068件・GPU107件成功。VSLは674件中671件成功、packed整数format非対応の3件skip。failure／errorは0。
- Typed／Allocation／BundleとAHOの関連compile契約121件成功。
- 変更C++33ファイル／21TUのstrict style違反0・未解析なし。追加した相互接続テストの1ファイル／1TUも再検査し違反0。MSBuild project／filtersのXML parse、diff checkも成功。
- サンプル--frames 120、--verifyとも成功。3 slots・12送信、UBO変化238831画素、resize1720x1080→2000x1360、pipeline生成1回、画像3枚保存を確認した。
- 最終ログは/private/tmp/aho-typed-master-final-{aho-cpu,aho-gpu,vsl,example,verify,contracts}.log。style reportはbuild/master/style-finalとstyle-interop。Windows／Linux実行は未検証。
