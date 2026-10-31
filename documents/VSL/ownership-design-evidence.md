# 所有・Impl配置設計の調査記録

2026-10-31。[設計書](ownership-and-placement.md)と[移行計画](ownership-migration-plan.md)の根拠・検証範囲を記録する。当初の設計調査の記録であり、その後の製品実装と検証は[Buffer配置の実装](buffer-placement.md)に記録する。ここにある寸法と未実装表記は当初checkoutの値。

## 調査対象

- checkout: `233717c6f5df9a21f97ba50c8d271530ef679b85`。
- 補足origin/master: `8959e5bf701677799b484999c4f55bcbdb5e9530`。
- StoragePolicy補足: `a09a2fd38524d1d34966863a983a64d198e40ee0`。
- rootの`AHOLi/CMakeLists.txt`の利用者変更は維持した。checkoutや製品コードは変更していない。
- rootの`AGENT.md`、`document/index.md`、所有／同期／Typedの関連資料を読み、資料の古い段階の説明と後続実装を区別した。

根拠の入口を以下にまとめる。行は調査基点での目安であり、実装着手時に再確認する。

| 基点 | ファイル／入口 | 確認事項 |
| --- | --- | --- |
| A | `vulkan/buffer.hpp`のBuffer constructor | template側からMakeBufferへ委譲 |
| A | `vulkan/buffer_and_image_accessor.hpp`のBufferAccessor | data_は共有所有。LocalBufferHolderのparentとownerは別 |
| A | `src/vulkan/buffer.cpp`のMakeBuffer（164行付近） | make_shared、native生成、memory type選択、allocate／bindの結合 |
| A | 同BufferImpl destructor（247行付近） | unmap→buffer破棄→device memory解放 |
| A | `vulkan/_pimpls.hpp`のBufferImpl、SubmissionImpl、GraphicResourceImpl | device所有、保持一覧、descriptor binding所有者 |
| A | `utils/typed_vertex_buffer.hpp`、`typed_resources.hpp` | viewのsnapshot所有、通常Bufferを内部生成 |
| A | `src/utils/typed_resources.cpp`のwrite_descriptor | native検証とbinding owner置換 |
| A | `src/vulkan/commands/copy_buffer.cpp` | 作成時Impl捕捉、invoke前検証、retain後のnative記録 |
| A | `src/vulkan/commands/bind_vertex_buffers.cpp` | owner取得経路と旧wrapper借用経路の差 |
| A | `src/vulkan/command.cpp`、`phase.cpp` | recording submission、完了保持、slot待機、cancel／送信／破棄 |
| A | `AHO/resource/buffer.hpp`のGPUBufferBase::replace | reallocate、検証、内容コピー後に共有stateを置換 |
| A | `AHO/src/command/buffer_upload.cpp` | invoke時にBufferBindingの現在値を取得 |
| B | `vulkan/resource_use.hpp`と対応cpp | weak submission追跡、groupの記録時binding追跡 |
| B | `utils/typed_resource_allocation.hpp` | 論理数量を計算。Impl寸法は含まない |
| B | `utils/typed_resource_bundle*.hpp`、`typed_frame_resources.hpp` | shared／frame構築、各Frameの共有所有 |
| B | `AHO/resource/typed_buffer.hpp` | native snapshotとCustom ownerをTyped viewへ接続 |
| C | `utils/typed_resource_storage.hpp` | 生成wrapper／factoryの選択。Impl配置は未分離 |

Aのパスは、特記しないものは`VSLi/VSL/include/VSL/`または`VSLi/VSL/`以下。AHO側は`AHOLi/`以下。固定commitへのリンクは設計書の基点表にある。

## compile-only寸法probe

実Impl定義を使った小さな独立CMakeプロジェクトを`/private/tmp/vsl-layout-design-probe`に作成した。VSL本体・AHO本体はbuildしていない。製品への機能追加でもない。

環境:

- macOS arm64、Clang 20.1.8、libc++、CMake 4.1.1、Ninja。
- C++26、MacOSX SDK、Vulkan headers 1.4.309.0、GLFW headers。
- Releaseは`-O1 -g0 -DNDEBUG`、Debugは`-O0 -g0 -DDEBUG`。
- `CMAKE_CXX_SCAN_FOR_MODULES=OFF`、`CMAKE_OSX_SYSROOT=macosx`。
- probeは`CheckTypeSize`と`CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`を使用。対象binaryを起動しない。

| 現checkoutの型 | Release size / alignment | Debug size / alignment |
| --- | --- | --- |
| BufferImpl | 72 / 8 | 72 / 8 |
| ImageImpl | 80 / 8 | 80 / 8 |
| PipelineImpl | 48 / 8 | 48 / 8 |
| SubmissionImpl | 88 / 8 | 88 / 8 |

これは**この環境の現在の実体**の数値。新設計のImpl寸法を定めるものでも、他ABIへ配布できる値でもない。Debug／Releaseの結果が同じことは、異なるcompiler／standard library／MSVC debug iterator設定でも同じという証拠にはならない。

取得値を埋め込んだ別TUで、各型のsizeofとalignofに対する8個のstatic_assertを両configでbuildした。Vulkan APIを呼ぶ必要はなかった。

負例としてBufferImplの期待sizeを72から73へ変えた別TUをsyntax-only compileし、`72 == 73`のstatic_assertで失敗することも確認した。これは実定義との不一致検出の確認であり、別artifactのABI guardを実装した検証ではない。

初回はsysroot未指定かつmodule scan既定の最小構成で`time.h`を発見できなかった。rootと同様にmodule scanを無効にし、MacOSX sysrootを明示して再configureした。生成工程にも製品と同じtoolchain／SDK条件が必要であることを示す。型が存在しないものとして手書き値へfallbackしない。

### 再現用のCMake例

これは調査専用。productionではconfig／arch／ABI fingerprint別の生成先、再生成依存、installとABI guardを追加する必要がある。

```cmake
cmake_minimum_required(VERSION 3.30)
project(VSLLayoutDesignProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
set(CMAKE_REQUIRED_INCLUDES
    "${VSL_CHECKOUT}/VSLi/VSL/include"
    "${VSL_VULKAN_INCLUDE}"
    "${VSL_GLFW_INCLUDE}")
set(CMAKE_EXTRA_INCLUDE_FILES "VSL/vulkan/_pimpls.hpp")
include(CheckTypeSize)
set(verification "#include <VSL/vulkan/_pimpls.hpp>\n")
foreach(kind BufferImpl ImageImpl PipelineImpl SubmissionImpl)
    check_type_size("vsl::impl::${kind}" "${kind}_SIZE"
        BUILTIN_TYPES_ONLY LANGUAGE CXX)
    check_type_size("char[alignof(vsl::impl::${kind})]" "${kind}_ALIGN"
        BUILTIN_TYPES_ONLY LANGUAGE CXX)
    if(NOT ${kind}_SIZE OR NOT ${kind}_ALIGN)
        message(FATAL_ERROR "Probe failed for ${kind}")
    endif()
    message(STATUS "${kind}: size=${${kind}_SIZE}, align=${${kind}_ALIGN}")
    string(APPEND verification
        "static_assert(sizeof(vsl::impl::${kind}) == ${${kind}_SIZE});\n"
        "static_assert(alignof(vsl::impl::${kind}) == ${${kind}_ALIGN});\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/verification.cpp" "${verification}")
add_library(layout-verification OBJECT
    "${CMAKE_CURRENT_BINARY_DIR}/verification.cpp")
target_include_directories(layout-verification PRIVATE ${CMAKE_REQUIRED_INCLUDES})
```

使用例（各pathは利用環境のものへ置換する）:

```sh
cmake -S /path/to/probe -B /tmp/vsl-probe-release -G Ninja \
  -DCMAKE_CXX_COMPILER=/path/to/clang++ \
  -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE='-O1 -g0 -DNDEBUG' \
  -DVSL_CHECKOUT=/path/to/AHO \
  -DVSL_VULKAN_INCLUDE=/path/to/Vulkan/include \
  -DVSL_GLFW_INCLUDE=/path/to/glfw/include
cmake --build /tmp/vsl-probe-release
```

Debugは別build directoryとDebug flagsでconfigureする。CheckTypeSizeの結果を同じcache名で使い回さない。サイズがarchitecture依存で0になるuniversal binaryの結果は、この簡易probeでは拒否する。

## 今回の検証と今後の検証の区別

今回確認したのは、静的な参照経路、compile-onlyでの寸法取得、別TUの寸法一致検査、文書のリンク／差分。設計したarenaの構築・GPU保持・cancelや循環検出を実装／実行したわけではない。

クロスコンパイル、MSVC、Linux、multi-config、インストールartifact不一致は未検証。これらの受入条件は移行計画へ記載している。新しい所有方式の実GPUテスト・sanitizer・性能測定は最初の移行単位以降で行う。
