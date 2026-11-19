# Linuxでのビルド

Linuxでは配布パッケージのVulkan loader・GLFWを使用します。Cocoa・Metal・MoltenVK向けのportability拡張はmacOSだけで使用します。GCC 14以上、CMake 3.30以上、VulkanヘッダーとSPIRV-Cross 1.3.239相当以上が必要です。自動Mesh reflectionは任意機能で、通常のビルドには不要です。

## Ubuntu 24.04の依存パッケージ

```sh
sudo apt-get update
sudo apt-get install gcc-14 g++-14 ninja-build git python3-venv \
  libvulkan-dev libglfw3-dev libspirv-cross-c-shared-dev libstb-dev \
  libgtest-dev glslc spirv-tools
```

`libspirv-cross-c-shared-dev`にはC++のcoreライブラリとCMake設定も含まれます。Boost・GLM・macOS SDKの導入は不要です。saitekikuukanとtinygltfはCMakeが固定リビジョンを取得するため、初回の構成時にはネットワーク接続が必要です。

Ubuntu 24.04標準のCMakeは3.30未満なので、既存のCMakeが古い場合はリポジトリルートで仮想環境に新しい版を用意します。

```sh
python3 -m venv build/tools
. build/tools/bin/activate
python -m pip install 'cmake>=3.30,<4'
cmake --version
```

Ubuntu 22.04標準のGCC 11ではビルドできず、Vulkanヘッダー・SPIRV-Crossも古いため更新が必要です。対応するツールチェーンと依存ライブラリを別途用意するか、新しいディストリビューションを使用してください。

## 構成・ビルド・テスト

リポジトリルートで実行します。

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --parallel 2
ctest --preset linux-debug --parallel 2
```

`linux-debug`はGCC 14・Ninjaを使い、ライブラリ、従来アプリ、テストを`build/linux-debug/`へ生成します。Releaseは`linux-release`で同様に構成・ビルドできます。サンプルは`./build/linux-debug/AHO/AHO`で起動でき、CMakeで設定したシェーダー・組み込みリソースのパスを使用するため、実行時の`AHO_HOME`指定は不要です。起動時のシェーダーコンパイルには`glslc`をPATHへ追加してください。

`Window::resize`はイベントを処理し、ウィンドウマネージャーがサイズ変更を反映するまで最長2秒待ちます。変更が反映されない場合や、待機中にウィンドウが閉じられた場合は`false`を返します。描画資源は反映後のサイズで更新します。

ライブラリだけをビルドする場合は、次のようにテストとサンプルを無効にできます。この構成ではGTest・glslc・spirv-toolsの検出を省略します。

```sh
cmake --preset linux-release -DAHO_BUILD_SAMPLE=OFF -DBUILD_TESTING=OFF
cmake --build --preset linux-release --parallel 2
```

別の対応コンパイラーを使う場合は、構成時に`-DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15`のように上書きし、`-B build/gcc15`で別のbuildディレクトリを指定してください。

stbは`libstb-dev`の`/usr/include/stb`、リポジトリの`thirdparty/`、または`AHO_STB_INCLUDE_DIR`で指定した場所から検出します。`stb_image.h`と`stb_image_write.h`を同じディレクトリに配置してください。

## 描画テスト

ウィンドウを使うテストにはX11またはWaylandのディスプレイとVulkanドライバーが必要です。画面のないCIでは、XvfbとMesaのソフトウェアVulkanドライバーを使用できます。最大化・復元の検査にはウィンドウマネージャーも必要なため、Openboxを起動します。

```sh
sudo apt-get install xvfb xauth openbox x11-utils mesa-vulkan-drivers vulkan-validationlayers
xvfb-run -a -s '-screen 0 1280x1024x24 -noreset' \
  bash tools/cmake/with_window_manager.sh ctest --preset linux-debug --parallel 2
```

CPUテストだけを実行する場合は、`ctest --preset linux-debug -L cpu -LE gpu --parallel 2`を使用します。
