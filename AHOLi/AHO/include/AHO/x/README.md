# AHOのexperimental機能

AHOのexperimental機能は、次のディレクトリに配置する。

- 公開ヘッダー: `AHOLi/AHO/include/AHO/x/`
- 実装: `AHOLi/AHO/src/x/`

`x/`以下は、ヘッダー側では`include/AHO/`、実装側では`src/`の既存の分類に合わせる。`core/math/`、`drawer/`、`engine/`、`io/`、`object/`、`pipeline/`、`resource/`、`utils/`など、必要な分類を使う。

対応するヘッダーとソースは、`x/`からの相対パスとファイル名を揃える。ヘッダーのみで実装する機能にはソースを作成する必要はない。

| 用途 | ヘッダー | ソース |
| --- | --- | --- |
| スクリーンショット | `include/AHO/x/screenshot.hpp` | `src/x/screenshot.cpp` |
| リソース関連の機能 | `include/AHO/x/resource/example.hpp` | `src/x/resource/example.cpp` |

上記は配置例。利用側は`<AHO/x/screenshot.hpp>`のように対象ヘッダーをインクルードする。

実装ファイルを追加するときは、`AHOLi/CMakeLists.txt`とVisual Studioの`AHOLi.vcxproj`・`AHOLi.vcxproj.filters`にも登録する。

空の分類ディレクトリは`.gitkeep`でGit上に保持する。ファイルを追加した分類では`.gitkeep`を削除してよい。

実装済みの機能: [glTF/GLB・画像・外部SPIR-Vの読み込み](../../../../../documents/AHO/experimental-gltf.md)、 [スクリーンショット](../../../../../documents/AHO/experimental-screenshot.md)。
