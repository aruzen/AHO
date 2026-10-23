# Quaternion・TRS・正射影

`core/math/quaternion.hpp`と`core/math/projection.hpp`は、モデルの姿勢・アニメーション補間・UI/2D表示で使う追加の数学APIを提供する。外部の数学ライブラリは必要ない。

## 4次元ベクトルと四元数代数

`d4::Vector<T>`は全成分xyzwを持つ4次元ベクトル。加減算、左右のスカラー倍、内積、成分積、型変換、ノルムと浮動小数点の正規化に対応する。`___AN4`で4番目の成分を参照できる。4次元の外積は提供しない。D4の追加は数学用Vectorの拡張であり、GPU画像の許容次元は1〜3のまま。

`Quaternion<T>`は`d4::Vector<T>`をpublic継承し、w + xi + yj + zkをxyzw順で保持する。既定構築は零元、`Identity()`は乗法単位元。構築とHamilton積で大きさを変更しない。基底の積はi²=j²=k²=ijk=-1、ij=k、ji=-k。手系は回転への写像で指定し、一般四元数の積の規約には混ぜない。

```cpp
constexpr aho::Quaternion<int> i{1, 0, 0, 0};
constexpr aho::Quaternion<int> j{0, 1, 0, 0};
static_assert((2 * i) * (2 * i) == aho::Quaternion<int>{0, 0, 0, -4});
static_assert(i * j == -(j * i));
```

加減算・Hamilton積・スカラー倍は結果の係数型を推論し、Quaternionを返す。`conjugate()`と`norm_squared()`は代数演算として提供する。係数は一括して浮動小数点へ限定せず、使用する演算の要件を適用する。順序比較は提供せず、等値は成分一致。可変の成分、Vectorとしての内積等は基底経由で利用できる。

四元数同士の除算演算子は提供しない。右から除す場合は`q * p.inverse()`、左から除す場合は`p.inverse() * q`と積の順を明示する。

`inverse()`は共役/ノルム平方。整数型では逆元と整数除算の切り捨てを混同しないよう提供せず、整数結果になるスカラー除算も提供しない。実係数の非零四元数では逆元が存在する。一般の可換係数体ではノルム平方が可逆であることが条件。正確な係数型では除算の意味は係数型の契約に従う。ノルム平方が0ならdomain_error。浮動小数点では最大成分で縮小してから計算し、零元・非有限成分はdomain_error、逆元が非有限になる場合はoverflow_error。

## 単位四元数による回転と補間

`UnitQuaternion<T, handed>`は単位Quaternionとして回転を表す。既定値は恒等回転。`UnitQuaternionF`と`UnitQuaternionD`はfloat/double・既定の左手系の別名。glTFの右手系を扱う場合は`UnitQuaternion<float, HandedCoordinate::RIGHT>`等を明示する。従来の#222の回転用Quaternionからはこちらへ移行する。`QuaternionF/D`は現在、一般四元数を指す。

```cpp
#include <AHO/core/math/quaternion.hpp>

const auto rotation = aho::UnitQuaternionF::FromAxisAngle(
    aho::Degree{90}, aho::d3::VectorF{0, 1, 0});
const auto halfway = aho::math::slerp(aho::UnitQuaternionF{}, rotation, .5f);
const auto direction = halfway.rotate(aho::d3::VectorF{0, 0, -1});
const auto restored = halfway.inverse().rotate(direction);
```

- `FromComponents(x,y,z,w)`はglTFと同じ成分順で受け取って正規化する。大きい値や小さい値でも、最大成分でスケールしてから長さを求める。
- `FromAxisAngle(Radian/Degree, axis)`は指定した手系の回転を作る。軸は正規化される。
- 成分・軸・角度が非有限、または成分/軸が全てゼロの場合はinvalid_argument。
- `components()`はx,y,z,w順の配列を値として返す。生成後の成分は直接変更できない。
- `FromQuaternion(q)`で一般四元数から明示的に正規化する。`quaternion()`は一般四元数へのconst参照であり、変更可能なVectorへの暗黙変換はない。
- 単位四元数の和・差・スカラー倍・スカラー除算は一般Quaternionを返し、単位制約を持つ型へ暗黙に戻さない。
- `q1 * q2`はq2→q1の順に回転する。合成時に再正規化し、繰り返し合成による長さのずれを抑える。
- `inverse()`は逆回転、`rotate()`はベクトルの回転、`to_matrix()`は#220と同じmat[row][column]・列ベクトル規約の行列を返す。
- `slerp<InterpolationPath::SHORTEST>()`は最短の回転経路を選ぶ。qと-qは同じ回転として扱い、ほぼ同一の向きでは正規化した線形補間へ切り替える。有限の補間率を受け取り、[0,1]外は外挿する。数値的に表現できない結果はQuaternion生成時に拒否する。

`slerp<InterpolationPath::DIRECT>()`では成分の符号を変えず、指定したQuaternion間の経路を補間する。対極または数値的に近すぎる対極成分の間は一意で安定した経路を定められないため、invalid_argumentで拒否する。既定経路はSHORTEST。異なる手系のQuaternion同士は暗黙に合成しない。

Euler角への変換、matrixからの回転抽出、TRS分解はこの追加には含めない。

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
