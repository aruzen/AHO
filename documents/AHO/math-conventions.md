# 数学APIの規約と配置

## コンパイル時の挙動選択

`core/math/conventions.hpp`のenumをテンプレート引数として受け取る。選択による実装の違いはif constexprで分ける。

| enum | 選択肢 | constexpr既定値 |
| --- | --- | --- |
| HandedCoordinate | LEFT / RIGHT | DEFAULT_COORDINATE_SYSTEM = LEFT |
| DepthRange | ZERO_TO_ONE / NEGATIVE_ONE_TO_ONE | DEFAULT_DEPTH_RANGE = ZERO_TO_ONE |
| ClipY | UP / DOWN | DEFAULT_CLIP_Y = UP |
| VectorConvention | COLUMN / ROW | DEFAULT_VECTOR_CONVENTION = COLUMN |
| MatrixLayout | ROW_MAJOR / COLUMN_MAJOR | DEFAULT_MATRIX_LAYOUT = ROW_MAJOR |
| ViewOrientation | UPRIGHT / ROLL_180 | DEFAULT_VIEW_ORIENTATION = UPRIGHT |
| InterpolationPath | SHORTEST / DIRECT | DEFAULT_INTERPOLATION_PATH = SHORTEST |

左手系のviewは前方+Z、右手系は前方-Z。正の回転角は各手系の規則に従い、左手系は右手系の逆向きに回転する。Vectorのcross自体は座標成分の行列式として従来どおり計算する。

```cpp
#include <AHO/core/math/projection.hpp>

auto view = aho::matrix::make_view<aho::HandedCoordinate::RIGHT>(
    aho::d3::PointF{0, 0, 3}, aho::d3::PointF{0, 0, 0}, aho::d3::VectorF{0, 1, 0});
auto projection = aho::matrix::make_perspective<
    aho::HandedCoordinate::RIGHT, aho::DepthRange::ZERO_TO_ONE, aho::ClipY::DOWN>(
        aho::Degree{66}, 1.6f, .1f, 1000.f);
auto gpu_matrix = aho::matrix::to_array<aho::MatrixLayout::COLUMN_MAJOR>(projection * view);
```

`Mat`自体は常に`mat[row][column]`。VectorConventionは式の意味を選び、MatrixLayoutは配列へ保存する順序を選ぶ。両者は独立。

- COLUMN: `M*v`。A*BはB→Aの順に作用し、translate→rotate→scaleはT*R*Sを作る。
- ROW: `v*M`。同じ幾何変換を転置で表し、合成時も積の順を変える。translate→rotate→scaleはS*R*Tになる。
- ROW_MAJOR/COLUMN_MAJORはto_array/from_arrayやinitializer-list factoryの入出力順序。ベクトル規約を自動的に変えない。

最適化されたアフィン逆行列も`inverse_affine<VectorConvention::ROW/COLUMN>()`で規約を選ぶ。一般の逆行列・転置は数値行列そのものへの演算なので、手系を指定しない。

## ヘッダーの責務

| ヘッダー | 内容 |
| --- | --- |
| mat.hpp | Mat型、traits、型エイリアス、演算子、stream出力 |
| matrix_functions.hpp | 汎用factory、identity、逆行列、転置、配列との相互変換 |
| functions.hpp | scalar/Point/Vectorの補間・距離・正規化・配列化 |
| transform.hpp | 移動・回転・拡縮・view生成、変換の合成、点/方向/法線の変換 |
| projection.hpp | 射影生成 |

`make_move(offset)`は移動行列を作る。`translate(matrix, offset)`は既存行列へ移動を合成する。make_rotation/rotate、make_scaling/scaleも同様の関係。生成と合成を別の役割として扱い、同じ生成処理を複製しない。

## 以前の関数からの移行

- make_look_atはmake_viewへ統合。
- make_perspective_zero_to_oneはmake_perspectiveへ統合し、DepthRangeを指定する。実行時のflip_yはClipYへ置換。
- to_column_major/from_column_majorはto_array/from_arrayへ統合し、MatrixLayoutを指定する。
- 旧make_viewの180度roll・row-vector表現は`make_view<RIGHT, ROLL_180, ROW>`で選択できる。
- 旧make_perspectiveは`make_perspective<RIGHT, NEGATIVE_ONE_TO_ONE, UP, ROW>`で再現できる。
- 旧make_moveのrow-vector表現は`make_move<ROW>`。
- 旧initializer-list factoryの列指定は`make<T, rows, columns, MatrixLayout::COLUMN_MAJOR>`。既定値とstd::array入力は行指定へ揃える。

従来のサンプルはこれらを明示して描画の契約を維持する。新しい呼び出しでは既定値を使うか、glTF等の入出力規約に合わせて明示する。
