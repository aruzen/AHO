# C++の言語標準

AHO・AHOLi・VSLと関連テストはC++26の言語モードでビルドする。CMakeは3.30以上を使用し、`CMAKE_CXX_STANDARD=26`と`CMAKE_CXX_STANDARD_REQUIRED=ON`で古い標準へのfallbackを禁止する。

Windowsの依存設定・ビルド・テスト手順は[Windowsでのビルド](windows.md)を参照。

AHOLi・VSL・AHO::ExperimentalMeshは`cxx_std_26`を利用側へ伝える。これらをtarget_link_librariesで使用するtargetもC++26以上のモードとなる。

現在のプロジェクトはnamed modulesを使わないため、CMakeの自動module走査はOFFにする。通常のビルドと、製品のビルド前に行うコンパイルDBベースの規約検査が、未生成のmodule応答ファイルへ依存しないようにする。

Clang/GCCの手動コンパイル契約は`-std=c++2c`、Visual Studioの各構成とMSVCのコンパイル契約は`/std:c++latest`を使用する。CMake経由の具体的なflagはCMakeがコンパイラーに合わせて選ぶ。

LLVM 20を既存の規約検査基盤として継続使用する。C++26モードで利用できる個々の言語・標準ライブラリ機能はツールチェーンの実装状況に従う。reflection等の機能は標準モードを上げただけでは利用可能にならないため、使用する際は対応コンパイラーとfeatureを別途確認する。

experimental MeshはGCC 16の`-freflection`で構造体から属性列を自動生成できる。`AHO::ExperimentalMesh`が対応判定とオプションの伝播を行い、`AHO_REQUIRE_MESH_REFLECTION=ON`で機能を必須にする。[Meshの説明](../experimental/mesh.md)を参照。GCCとClangを切り替える場合はbuildディレクトリを分け、GTestなどのC++依存ライブラリも標準ライブラリを揃える。

macOSのCocoaによる画面サイズ取得は、Apple ClangでコンパイルするObjective-Cファイルへ分離する。GCCが解析できないApple SDKのBlocks構文をC++側へ持ち込まず、整数値をCの関数で受け渡す。CMakeはmacOSでObjective-Cを有効にし、GCC使用時はこの部分に`clang`を選ぶ。VulkanのloaderはC ABIなので共用できるが、SDK付属のClang製SPIRV-CrossアーカイブはGCC用に再ビルドする必要がある。

macOSではCMakeが既定のAppleClangを選ぶ場合がある。Homebrew LLVM 20を使う場合はconfigure時に明示する。

```sh
cmake -S . -B build \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm@20)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm@20)/bin/clang++" \
  -DAHO_HOME="$PWD/AHOLi/AHO" -DVSL_HOME="$PWD/VSLi/VSL"
```

参照: [CMakeのcxx_std_26](https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html)、[ClangのC++26対応](https://clang.llvm.org/cxx_status.html)、[MSVCの/std](https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170)。
