# Quaternion・TRS・正射影

`core/math/quaternion.hpp`と`core/math/projection.hpp`は、モデルの姿勢・アニメーション補間・UI/2D表示で使う追加の数学APIを提供する。外部の数学ライブラリは必要ない。

## 回転と補間

`Quaternion<T, handed>`は単位Quaternionとして回転を表す。既定値は恒等回転。`QuaternionF`と`QuaternionD`はfloat/double・既定の左手系の別名。glTFの右手系を扱う場合は`Quaternion<float, HandedCoordinate::RIGHT>`等を明示する。

```cpp
#include <AHO/core/math/quaternion.hpp>

const auto rotation = aho::QuaternionF::FromAxisAngle(
    aho::Degree{90}, aho::d3::VectorF{0, 1, 0});
const auto halfway = aho::math::slerp(aho::QuaternionF{}, rotation, .5f);
const auto direction = halfway.rotate(aho::d3::VectorF{0, 0, -1});
const auto restored = halfway.inverse().rotate(direction);
```

- `FromComponents(x,y,z,w)`はglTFと同じ成分順で受け取って正規化する。大きい値や小さい値でも、最大成分でスケールしてから長さを求める。
- `FromAxisAngle(Radian/Degree, axis)`は指定した手系の回転を作る。軸は正規化される。
- 成分・軸・角度が非有限、または成分/軸が全てゼロの場合はinvalid_argument。
- `components()`はx,y,z,w順の配列を値として返す。生成後の成分は直接変更できない。
- `q1 * q2`はq2→q1の順に回転する。合成時に再正規化し、繰り返し合成による長さのずれを抑える。
- `inverse()`は逆回転、`rotate()`はベクトルの回転、`to_matrix()`は#220と同じmat[row][column]・列ベクトル規約の行列を返す。
- `slerp<InterpolationPath::SHORTEST>()`は最短の回転経路を選ぶ。qと-qは同じ回転として扱い、ほぼ同一の向きでは正規化した線形補間へ切り替える。有限の補間率を受け取り、[0,1]外は外挿する。数値的に表現できない結果はQuaternion生成時に拒否する。

`slerp<InterpolationPath::DIRECT>()`では成分の符号を変えず、指定したQuaternion間の経路を補間する。対極または数値的に近すぎる対極成分の間は一意で安定した経路を定められないため、invalid_argumentで拒否する。既定経路はSHORTEST。異なる手系のQuaternion同士は暗黙に合成しない。

この型は回転用であり、非単位Quaternionの一般代数を表す型ではない。Euler角への変換、matrixからの回転抽出、TRS分解はこの追加には含めない。

## TRS合成

```cpp
auto model = aho::matrix::make_trs(
    aho::d3::VectorF{10, 0, 0}, rotation, aho::d3::VectorF{1, 2, 1});
auto gpu_model = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(model);
```

`make_trs(translation, rotation, scaling)`は既定の列ベクトルではT*R*Sを返す。`make_trs<VectorConvention::ROW>()`ではその転置表現を返す。頂点には拡縮→回転→平行移動の順に作用する。回転行列の各列へ拡縮を反映し、平行移動の列を設定するため、この合成に汎用4×4行列積を重ねる必要はない。

平行移動・拡縮は有限値を要求する。負の拡縮やゼロの拡縮も許可する。ゼロ拡縮によって逆行列を作れない場合は、既存inverse_affineの失敗結果を扱う。非一様拡縮後の法線には#220のtransform_normalを使う。

## 正射影

```cpp
#include <AHO/core/math/projection.hpp>

auto projection = aho::matrix::make_orthographic<aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(
    0.f, 800.f, 0.f, 600.f, 0.f, 100.f);
```

引数はleft/right/bottom/top/near/far。HandedCoordinate、DepthRange、ClipY、VectorConventionをtemplate引数で指定する。上の右手系・depth[0,1]の例では、z=-nearがdepth 0、z=-farがdepth 1。左手系では前方+Z、NEGATIVE_ONE_TO_ONEではnearのdepthが-1になる。nearは0や負値も使え、境界は有限かつleft<right、bottom<top、near<farを要求する。

ClipY::UPでは左下が(-1,-1)、右上が(1,1)。DOWNでは平行移動成分も含めY行を反転する。この例の座標はY-upなので、原点(0,0)は画面左下になる。左上原点のUI座標を使う場合は、UI側の座標規約に合わせてClipYを選ぶ。

大きい対称境界でも境界の和や差を直接計算せずoverflowを避ける。選択したscalar型で有限の行列を表現できないほど狭い範囲はinvalid_argumentで拒否する。

GPUへの転送にはto_array<MatrixLayout::COLUMN_MAJOR>を使用する。mat[row][column]、行/列ベクトル、配列の保存順は[数学規約](math-conventions.md)を参照。
