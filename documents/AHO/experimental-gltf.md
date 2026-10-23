# glTF/GLBの読み込み（experimental）

公開APIは`<AHO/x/resource/gltf.hpp>`と`<AHO/x/resource/shader.hpp>`、名前空間は`aho::x::resource`。
GPUやwindowを作らずにモデルを読み込み、描画側へ渡せる配列と指定時刻のposeを取得する。

```cpp
#include <AHO/x/resource/gltf.hpp>
#include <AHO/x/resource/shader.hpp>

using namespace aho::x::resource;
auto model = GltfModel::load("assets/scene.glb");
auto pose = model.animations().empty() ? model.restPose() : model.sample(0, 1.25);
for (std::size_t i = 0; i < model.nodes().size(); ++i) {
    const auto& node = model.nodes()[i];
    if (node.mesh < 0) continue;
    const auto& mesh = model.meshes()[node.mesh];
    const auto& world = pose.worldMatrices[i];
    auto joints = model.skinMatrices(pose, i); // skinなしなら空
    // mesh.primitivesのvertices/indices、world、jointsを描画側へ渡す。
}
auto image = loadImage("assets/albedo.png"); // RGBA8を所有
```

`GltfModel`は読込データを共有する不変asset。コピーしてもメッシュや画素を複製しない。
各spanはassetが生存する間有効。複数instanceで異なる時刻の`GltfPose`を独立に評価できる。
描画対象のnodeは、選択したsceneの`roots`から辿って決める。
`GltfPose`は配列を所有する。`skinMatrices`には同じassetから取得したposeを渡す。

## 対応範囲

- glTF 2.0の`.gltf`/`.glb`。外部buffer、data URI、GLB内bufferと画像。
- POSITION/NORMAL/TANGENT、TEXCOORD_0/1、COLOR_0、JOINTS_0/WEIGHTS_0。
- interleaved buffer、sparse accessor、正規化整数のUV/色/weight、8/16/32bit index。
- 非indexed primitiveには連番indexを生成。point/line/triangleの7種のtopologyを維持する。
- PBR metallic/roughnessとbase color・normal・occlusion・emissiveのtexture参照、alpha mode、両面設定。
- PNG/JPEGをRGBA8へ展開。16bit入力は8bitへ変換。画像名・texture/sampler参照を維持。
- `KHR_materials_unlit`、`KHR_texture_transform`（offset/scale/rotation/texCoord）。
- scene/root、node階層、matrixまたはTRS、skinとinverse bind matrix。
- translation/rotation/scale animationのLINEAR/STEP/CUBICSPLINE。LINEAR回転は最短経路のslerp、CUBICSPLINEは区間秒数を掛けたHermite補間後に回転を正規化。

meshの`material=-1`はglTF既定material。欠けたnormal/tangentは`hasNormals/hasTangents=false`となり、vertex内の既定値はプレースホルダー。flat normalやMikkTSpace tangentの生成は描画側で行う。
JOINTS_0/WEIGHTS_0の4 influenceに対応し、読み込み時にweightを正規化する。

matrixはglTFのcolumn-major/列ベクトル、右手系・Y-upを維持する。ローカルは`T*R*S`、worldは`parent*local`。
`GltfMatrix`は`std::array<float,16>`であり、AHOの別の行列型へ渡す際は配置を確認する。
画素の上下反転・色空間変換は行わない。baseColor/emissiveはsRGB、metallicRoughness/normal/occlusionはlinearとして描画側で扱う。同じimageを異なる色空間で参照する場合もある。
texture transformの適用順は`offset + R(rotation) * (scale * uv)`。

`sample(animation, time, loop)`のtimeはglTFのclip時刻（秒）。loop時は`[start,end)`へwrapし、負時刻にも対応する。非loop時はclip範囲へclampする。各trackは自身の始終点でclampする。
`skinMatrices`はmesh nodeごとに`inverse(meshWorld) * jointWorld * inverseBind`を返す。meshWorldが特異なら例外。

## シェーダー

glTF 2.0はshaderコードを内包しないため、モデルのmaterialと外部SPIR-Vを別々に読む。

```cpp
auto vertexWords = aho::x::resource::loadSpirv("shaders/model.vert.spv");
auto fragmentWords = aho::x::resource::loadSpirv("shaders/model.frag.spv");
// 既存VSLでGPU moduleを作る場合:
// vsl::Shader<vsl::ShaderType::Vertex> vertex(device, "shaders/model.vert.spv");
// vsl::Shader<vsl::ShaderType::Fragment> fragment(device, "shaders/model.frag.spv");
```

`loadSpirv`はlittle-endianのSPIR-V 1.0〜1.6をword配列へ読み、ヘッダーと命令長を検査する。
shader意味論の検証、GLSLコンパイル、GPU upload、PBR/skin shaderの自動生成や描画pipeline構築は含まない。読み込んだvertex/画像/material/poseに合うshaderとpipelineを利用側で用意する。

## 依存とビルド

tinygltfのC++版v2.9.7、commit `488a70a3df62a4df1a736e9e56fb8836580c4888`を使用。
CMakeは初回にSHA256固定の公式archiveを取得する。オフラインでは同版を展開し、
`-DAHO_TINYGLTF_INCLUDE_DIR=/path/to/tinygltf`を指定する（tiny_gltf.h/json.hpp/stb_image.hを含むディレクトリ）。
既存の他のAHO/VSL依存は従来どおり必要。

Visual Studioでは同archiveを`thirdparty/tinygltf/`へ展開するか、`AHOTinyGltfRoot`プロパティで場所を指定する。
新規2cppはPCHを利用せず、tinygltf/stbの実装マクロはgltf.cppの1箇所に限定する。stb関数はTU内に閉じ、既存の画像ローダーと重複定義しない。

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DAHO_BUILD_GLTF_EXAMPLE=ON
cmake --build build --target aho-gltf-inspect
./build/AHOLi/aho-gltf-inspect assets/scene.glb 1.25 shaders/model.vert.spv
cmake --build build --target aho-test
ctest --test-dir build --output-on-failure -R 'GltfTest|ShaderAssetTest'
```

## 未対応とエラー

morph target/weight animation、4を超えるskin influence、Draco/meshopt圧縮、KTX2/BasisU、その他の必須拡張には対応しない。非対応の必須拡張・morph等は黙って落とさず例外にする。任意の未知拡張は無視して`warnings()`へ記録する。camera/lightの描画設定は生成しない。
境界外のbuffer/accessor/index、循環・複数親node、非有限値、不正なanimationは読み込みに失敗する。`sample`のindex不正は`out_of_range`、時刻不正は`invalid_argument`、ファイル/形式エラーは`runtime_error`。全glTF schemaやSPIR-Vの完全validatorではない。


参照: [tinygltf v2.9.7](https://github.com/syoyo/tinygltf/tree/v2.9.7)、
[glTF 2.0仕様](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc)。
