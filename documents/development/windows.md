# Windowsでのビルド

Visual Studio 2026のC++デスクトップ開発ワークロード、MSVC v145 x64、CMakeを使用する。検証環境はMSVC 19.51、CMake 4.4、Vulkan SDK 1.4.304.0。言語モードはmasterと同じC++26で、MSVCでは`/std:c++latest`を使う。reflectionを必須にする構成には対応コンパイラーが別途必要になる。[言語標準](cxx-standard.md)も参照。

## 依存ライブラリ

`VULKAN_SDK`にWindows版SDKのルートを設定する。Vulkan、GLM、SPIRV-CrossのDebug/Releaseライブラリ、`glslc`、`spirv-as`を使用する。SDKの`Bin`をPATHへ追加する。

GLFWは3.4以上の共有ライブラリを使用する。`GLFW_ROOT`（互換設定は`GLFW`）にWindows配布物を指定できる。ヘッダーが見つからない場合、または3.3以前の場合、CMakeがGLFW 3.4の固定リビジョンを取得してビルドする。実行ファイルと同じ構成の`glfw3.dll`をビルド後にコピーする。

saitekikuukanとTinyGLTFは既存のCMake設定が指定するリビジョンを使用する。saitekikuukanは`0e608984951e4f243f7a8f506c16fd7840998800`。依存先のmasterを追いかける設定にはしない。

テストにはGoogleTestが必要。`thirdparty/googletest`にソースがあれば同じMSVC/CRTでビルドする。別の場所は`AHO_GOOGLETEST_ROOT`で指定でき、ソースがなければ`find_package(GTest)`を使用する。既存CIと揃える場合の準備例:

```powershell
git clone https://github.com/google/googletest.git thirdparty/googletest
git -C thirdparty/googletest checkout 52eb8108c5bdec04579160ae17225d66034bd723
Invoke-WebRequest https://raw.githubusercontent.com/nothings/stb/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5/stb_image.h -OutFile thirdparty/stb_image.h
Invoke-WebRequest https://raw.githubusercontent.com/nothings/stb/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5/stb_image_write.h -OutFile thirdparty/stb_image_write.h
```

stbの2ヘッダーはサンプルと画像保存にも使用する。`thirdparty`はGit管理対象外。

## CMakeとテスト

リポジトリのルートで、VS 2026のDeveloper PowerShellから実行する。コンパイル契約テストは`cl.exe`を直接起動するため、CTestにもMSVCのINCLUDE/LIB環境が必要。

```powershell
$env:Path = "$env:VULKAN_SDK\Bin;$env:Path"
cmake --preset windows-vs2026-debug
cmake --build --preset windows-vs2026-debug --parallel 4
ctest --preset windows-vs2026-debug --parallel 4
cmake --build --preset windows-vs2026-release --parallel 4
ctest --preset windows-vs2026-release --parallel 4
```

configure presetはDebug/Release共通で`build/vs2026`を生成し、`AHO_HOME`と`VSL_HOME`をそのcheckoutへ固定する。生成物は`build/vs2026/lib/<構成>/AHOLi.lib`と`VSL.lib`、サンプルは`build/vs2026/AHO/<構成>/AHO.exe`。テストのビルドだけなら`windows-vs2026-tests`を使える。GPUテストはVulkan対応GPUとデスクトップ環境を使用する。

MSVCでCMakeの`cxx_std_26`情報がまだ提供されない場合は、`/std:c++latest`へ対応づける。古いC++標準へのfallbackは行わず、reflectionなど個別機能の判定も維持する。

コンパイル契約は失敗の有無と理由を確認する。日本語版MSVCしかない環境でも実行できるよう、必要な診断番号を理由判定へ対応づける。入力の単体テストでは、GLFWのDLL importを外して既存のCPU用バックエンドを呼ぶ。

## Visual Studioのソリューション

CMakeで依存の取得とビルドを済ませてから`AHO.sln`を開き、x64のDebug/Releaseを選ぶ。`Windows.props`が共有設定を提供し、ProjectReferenceでVSLi、AHOLi、AHOの依存順とリンクを管理する。出力先は`x64/<構成>`。

`Windows.props`はCMakeが生成したGLFW 3.4の同じ構成を優先する。外部のGLFWを使う場合は`GLFW_ROOT`、必要に応じて`GLFWLibraryDir`と`GLFWIncludeDir`を指定できる。saitekikuukanとTinyGLTFも標準の`build/vs2026/_deps`を参照し、独自の取得先は`AHO_SAITEKIKUUKAN_INCLUDE_DIR`と`AHO_TINYGLTF_INCLUDE_DIR`で変更できる。

複数checkoutを使う場合、環境変数`AHO_HOME`と`VSL_HOME`が別checkoutを指していないことを確認する。未指定ならソリューション自身のソースを使う。古いソースへ新しいlibだけをコピーせず、ヘッダー・ソース・libを同じリビジョンに揃える。

デバッガーの作業ディレクトリは`AHO`で、`AHO_HOME`とSDKのツール検索パスも設定する。コマンドラインからサンプルを起動する場合も同じ作業ディレクトリと環境を使用する。

## 2026-10-28の検証結果

master `17950f77c738db5c7bf6af1c08cc4973e907513f`を取り込んだ状態で、CMakeと`AHO.sln`のx64 Debug/Releaseビルドが成功。サンプルは両構成で起動し、通常のウィンドウ終了操作で終了コード0、Vulkan検証エラーなしを確認した。ソースパス225件の検査と、変更したC++ファイルのclang-format 20検査も成功。LLVM開発ライブラリを使うAST規約検査は未実行。

Debug/Releaseの全2127テストを実行し、修正対象の再実行後も次の8件が未解決。GPUが対応しないpacked integer形式の3件はskipした。テストの期待値や有効・無効の設定は維持している。

| 対象 | 件数 | 確認した失敗 |
| --- | ---: | --- |
| `MatScaledInverseTest.ExactlyDependentIntegerRowsRemainSingularAfterRescaling<float>` | 1 | rescale後の特異行列を受理してしまう |
| `Compile.Line.PolygonAddition`、`PtrLine.PolygonAddition`、`Triangle.POLYGON_ADDITION`、`PtrTriangle.POLYGON_ADDITION` | 4 | 公開ヘッダーからのコンパイルでPolygonが不完全型になる |
| `Compile.Drawer.StandardTuple`、`Link.Drawer.Standard` | 2 | drawの引数数の不一致、および宣言されたdrawのリンク先がない |
| `SwapchainWindowIntegrationTest.NativeCreationFailureIsReportedBeforeQueryingImagesAndAllowsValidRetry` | 1 | このSDK/ドライバー環境では意図的なvalidation abortが期待1回に対して8回通知される |

Windowsの隠しウィンドウ用テスト環境は装飾を外す。装飾付きではGLFW単体でも要求した幅96pxが120pxに制限されるためで、既存の寸法検証はそのまま使用する。日本語MSVC向けの診断判定や、並列コンパイル検査のPDB共有回避も実行基盤側で行う。
