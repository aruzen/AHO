# sample-gameの数学処理をAHOへ移す

`core/math/functions.hpp`と`core/math/transform.hpp`はGLMを使わず、既存のPoint・Vector・Matを使う。CPU数学処理はGPUデバイスを必要としない。

## GLM呼び出しとの対応

#216の`sample-game/src/main.cpp`・`world.hpp/cpp`、`x/scene/mesh_renderer`・`gltf_renderer`・Cameraで使うCPU数学処理を対象とする。

| 元の処理 | AHO側での扱い |
| --- | --- |
| `vec2` / `vec3` | 位置は`d2/d3::PointF`、方向・差・色の成分演算は`d2/d3::VectorF`。座標アクセスは`value.___AN1`等。 |
| `vec4`、`vec3 + w` | uniformや同次座標は`std::array<float, 4>`。`math::to_array`で座標を取り出してwを付ける。 |
| `dot` / `cross` / `length` | 既存Vectorの同名メンバー関数。 |
| `normalize` | 通常は既存`get_normalized()`。入力検証が必要なら`math::normalized()`。ゼロ長・非有限の長さを拒否する。 |
| `distance` | `math::distance()`。同じ型・座標系のPoint/Vectorで使用可能。 |
| `mix` | `math::lerp()`。scalar、Point/Vector、固定長配列。範囲外の補間率は外挿する。 |
| `smoothstep` | `math::smoothstep()`。昇順かつ有限のedgeを要求し、範囲外は0/1へclampする。 |
| `mat4(1)` | 既存`matrix::make_identity<float, 4>()`。 |
| `translate` / `rotate` / `scale` | `matrix::translate/rotate/scale()`。右側へ変換を合成する。回転には既存Degree/Radianを使う。 |
| `value_ptr`による行列コピー | `matrix::to_array<MatrixLayout::COLUMN_MAJOR>()`で所有する配列を返す。glTFの行列を読むときは`from_array<float, 4, 4, MatrixLayout::COLUMN_MAJOR>()`。 |
| 独自の行列積 | 既存Matの`operator*`。column-major配列のまま乗算せず、先にfrom_arrayする。 |
| 独自の行列による点変換 | `matrix::transform_point()`、方向は`transform_vector()`。 |
| shaderへ渡す逆転置・法線変換 | `matrix::transpose()`、`transform_normal()`。逆行列は既存inverse_affineを再利用し、失敗理由をexpectedで返す。正規化は呼び出し側で行う。 |
| Cameraのview | `matrix::make_view<HandedCoordinate::RIGHT>()`。右手系の通常のview。 |
| Cameraの射影要素の手動補正 | `matrix::make_perspective<HandedCoordinate::RIGHT, DepthRange::ZERO_TO_ONE>()`。右手系・-Z視線・depth[0,1]。Vulkanの画面向きには`ClipY::DOWN`を指定する。 |
| 射影後の座標 | `matrix::project_point()`で透視除算。w=0や非有限wを拒否する。 |
| `pi` / `two_pi` | `std::numbers::pi_v<T>` / `2 * std::numbers::pi_v<T>`。 |
| `sin` / `cos` / `atan2` / `sqrt` / `exp` / `ceil` / `abs` / clamp等 | 引き続き標準ライブラリを使う。 |

GLSL内の組み込み関数や、地形・木の生成、湖や障害物との衝突、歩行速度・視点の揺れ、昼夜の制御はサンプル側に置く。glTFファイルのdecodeやanimationのデータ管理も数学ライブラリへ含めない。

## 行列の規約

新しいtransform APIは`mat[row][column]`、列ベクトル、右手系を採用する。`A * B`はB→Aの順に作用する。`translate`→`rotate`→`scale`の呼び出しは`T * R * S`となるため、頂点には拡縮→回転→平行移動の順で作用する。

GPUへ渡すときは`to_array<MatrixLayout::COLUMN_MAJOR>()`を使う。この結果はGLSLの既定のmat4とglTFの行列配列に対応する。Mat内部の二重配列を`float*`へキャストしたり、Point/Vectorのメモリをそのまま頂点属性とみなしたりしない。転送用頂点には`std::array<float, 3>`等を使い、stride/offsetを明示する。

既定の手系は左手系。sample-game/glTFの右手系を使う経路では明示的にRIGHTを指定する。Matからの非メンバー関数の移動先と、旧規約を再現するtemplate引数は[数学APIの規約](math-conventions.md)を参照する。

## 地形・モデル・カメラの使用例

```cpp
#include <AHO/core/math/projection.hpp>

auto model = aho::matrix::make_identity<float, 4>();
model = aho::matrix::translate(model, aho::d3::VectorF{4, 2, 81});
model = aho::matrix::rotate<aho::HandedCoordinate::RIGHT>(model, aho::Radian{.3f}, aho::d3::VectorF{0, 1, 0});
model = aho::matrix::scale(model, aho::d3::VectorF{.014f, .014f, .014f});
auto fox_world = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(model);

const auto view = aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(
    aho::d3::PointF{0, 3, 87}, aho::d3::PointF{0, 3, 86}, aho::d3::VectorF{0, 1, 0});
const auto projection = aho::matrix::make_perspective<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(
    aho::Degree{66}, 1.6f, .1f, 1600.f);
auto view_projection = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(projection * view);

const auto normal = aho::math::normalized(
    (aho::d3::PointF{1, 0, 0} - aho::d3::PointF{0, 0, 0}).cross(
        aho::d3::PointF{0, 0, 1} - aho::d3::PointF{0, 0, 0}));
auto vertex_normal = aho::math::to_array(normal);
const auto snow = aho::math::smoothstep(60.f, 145.f, 100.f);
auto color = aho::math::lerp(aho::d3::VectorF{.14f, .35f, .27f}, aho::d3::VectorF{.72f, .84f, .8f}, snow);
```

Quaternion、slerp、TRS animationの共通化は、このサンプルが使うGLM関数の範囲にはない。既存glTF loaderの個別実装を移す際に別途扱う。GLMを新しい公開ヘッダーやビルド依存へ追加しない。
