# インストールしたAHOの利用

AHO/VSLをビルドし、`cmake --install build --prefix <配置先>`で配置します。利用側は依存のインストール先を`CMAKE_PREFIX_PATH`等で指定し、以下だけを記述します。

```cmake
find_package(AHO CONFIG REQUIRED)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE AHO::AHOLi)
```

公開includeとC++26要件はtargetから伝播します。AHOConfigがVSL・saitekikuukanを、VSLConfigがVulkan・glfw3・SPIRV-Cross coreを解決します。MSDF付きの配置ではmsdfgenも必要です。VSLだけを使う場合は`find_package(VSL CONFIG REQUIRED)`と`VSL::VSL`を使います。開発ツリー内でも同じtarget名を使えます。

VulkanやGLFWのライブラリをAHOと同じディレクトリへコピーする必要はありません。CMakeで見つかる各packageを使います。SPIRV-Crossは利用するC++コンパイラーと標準ライブラリのABIを揃えてビルドしてください。たとえばmacOSのGCCとHomebrewのClang製C++ライブラリは混在させません。

`python3 tools/cmake/check_installed_package.py --build-dir build`で、配置先の移動、saitekikuukanの別prefixへの分離、AHO::AHOLiだけにリンクするconsumerのコンパイル・リンク・実行を検証できます。公開includeの手動追加やVSLの先行find_packageは行いません。検証用の配置とconsumerはbuild内の`package-check-*`に残ります。追加のツールチェーン指定は`--cmake-arg=-D名前=値`で渡せます。
