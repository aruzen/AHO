# AHO

AHOはC++の描画・リソースライブラリです。Vulkanを扱うVSLの上に、Window、Buffer、Mesh、入力、実験的なglTF・Screenshot・MSDF等を提供します。

## 開発の入口

通常はルートの`CMakeLists.txt`から構成します。

| 場所 | 役割 |
| --- | --- |
| `AHOLi/AHO/include/AHO`・`src` | AHOライブラリ。targetは`AHOLi` / `AHO::AHOLi` |
| `VSLi/VSL/include/VSL`・`src` | GPU制御ライブラリ。targetは`VSL` / `VSL::VSL` |
| `AHO/Main.cpp` | 従来の動作確認アプリ。targetは`AHO` |
| `examples`・`AHOLi/AHO/examples` | 任意で有効にする利用例 |
| `documents` | API・設計・ビルドの文書 |
| `AHO.sln` | Windowsの既存MSBuild開発経路 |

実行ファイルやbuildディレクトリは生成物です。各buildディレクトリに出力し、Gitには登録しません。

## CMakeでのビルド

CMake 3.30以上、Ninja等の生成ツール、C++26対応コンパイラー、Vulkan SDK/loader、GLFW、SPIRV-Cross core、Boostを用意します。通常アプリはGLM、テストはGTestも必要です。saitekikuukanとtinygltfは固定リビジョンを取得します。MSDFを有効にする場合はFreeType等も必要です。

macOSで自動reflectionを利用する場合はGCC 16を使います。C++依存ライブラリ、とくにGTest・SPIRV-Crossは同じ標準ライブラリのABIでビルドし、その配置先を`CMAKE_PREFIX_PATH`で指定してください。SDK指定を含む詳細は[C++標準とツールチェーン](documents/development/cxx-standard.md)を参照してください。

```sh
cmake -S . -B build/gcc16 -G Ninja \
  -DCMAKE_C_COMPILER=gcc-16 -DCMAKE_CXX_COMPILER=g++-16 \
  -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/gcc-deps \
  -DAHO_ENABLE_EXPERIMENTAL_MESH=ON -DAHO_REQUIRE_MESH_REFLECTION=ON
cmake --build build/gcc16 --target AHOLi VSL --parallel 2
```

`AHO_HOME`・`VSL_HOME`の指定は不要です。従来の`-D`指定と環境変数による指定も使えます。`-D`指定、環境変数、ソースツリーの既定値の順に優先します。

| 設定 | 用途 |
| --- | --- |
| `AHO_BUILD_SAMPLE=OFF` | 従来のアプリを構成しない。別projectからのadd_subdirectoryでは既定OFF |
| `BUILD_TESTING=OFF` | テストとGTestの検出を省略 |
| `AHO_BUILD_TYPED_EXAMPLE=ON` | Typed描画サンプル |
| `AHO_BUILD_GLTF_VIEWER=ON` | glTF描画サンプル |
| `AHO_BUILD_GLTF_EXAMPLE=ON` | glTFのCPU側検査サンプル |
| `AHO_ENABLE_MSDF=ON` | 実験的なCPU MSDF機能 |

従来アプリ・Typedサンプルで使うstbヘッダーは[thirdparty/setup.md](thirdparty/setup.md)に従って配置します。コンパイラーを変える場合はbuildディレクトリを分けてください。

`cmake --build build/gcc16 --target build-tests`でテストをビルドし、`ctest --test-dir build/gcc16 --output-on-failure`で実行します。GPUテストには利用可能なGPUとウィンドウ環境が必要です。

## ライブラリとして使う

配置方法と`find_package(AHO)`の例は[インストールpackage](documents/installed-package.md)、CPU最適化等は[ビルドオプション](documents/build-options.md)、APIと設計は[文書一覧](documents/README.md)を参照してください。
