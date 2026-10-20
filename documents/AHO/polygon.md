# Polygon

`AHO/core/polygon.hpp` の `Polygon<R, D, CI, Size, Alloc>` は頂点と三角形インデックスを所有する値型です。既存の前方宣言のテンプレート引数を維持します。座標次元・座標情報を保持し、2D/3Dで利用できます。

```cpp
using P = aho::d2::PointF;
const std::array points{P(0, 0), P(1, 0), P(0, 1)};
const std::array indices{aho::IndexedTriangle<std::size_t>{{0}, {1}, {2}}};
aho::Polygon<float, vsl::D2> mesh(points, indices);
for (const auto triangle : mesh.triangles()) { /* 描画など */ }
```

構築時に全インデックスとSizeの表現範囲を検証します。範囲外のインデックスはstd::out_of_range、頂点数がSizeの表現範囲を超える場合はstd::length_errorです。空のメッシュは有効です。入力の配列はコピーするため、呼び出し元の寿命に依存しません。

points()/indices()はconst span、triangles()/lines()は読み取り用の遅延ビューです。ビューは元のPolygonより長く保持できず、元の再代入・move・破棄で無効になります。lines()は三角形ごとの辺を返し、共有辺を重複して含みます。area()は各三角形の面積の合計で、重なりも加算します。

メッシュは任意の三角形集合を受け付け、自己交差・重複・退化した三角形を禁止しません。輪郭からの構築とは契約を分けます。従来Line/Triangleの仮実装operator+の意味の変更は含めません。

## 輪郭の三角形分割

`<AHO/core/triangulation.hpp>`の`triangulate<Size = std::size_t>(points)`は、輪郭順の2D点列を`std::expected<Polygon<...>, TriangulationFailedCause>`として返します。input_rangeを受け、時計回り・反時計回りを維持します。終点の始点繰り返しは除去し、直線上の中間頂点は三角形の参照から外します。元の頂点番号を維持するため、未参照の頂点が残る場合があります。

```cpp
const std::array contour{P(0, 0), P(3, 0), P(3, 1), P(1, 1), P(1, 3), P(0, 3)};
auto result = aho::triangulate(contour);
if (!result) { /* result.error()で原因を判別 */ }
```

穴のない単純な凹多角形をear clippingで分割します。点不足・非有限値・重複頂点・自己交差・退化・インデックス範囲超過をエラーとして返します。メモリ確保の失敗はstd::bad_allocです。自己交差検査はO(n²)、ear clippingの最悪時間はO(n³)で、大量頂点用の高速化は含みません。

判定はlong doubleへ変換し、座標を共通係数で縮小してから外積を計算します。方向判定の許容値は演算項の大きさ×long doubleのepsilon×16です。厳密述語ではなく、極端な縦横比・ほぼ共線な点・long doubleの精度を超える整数について厳密な判定は保証しません。
