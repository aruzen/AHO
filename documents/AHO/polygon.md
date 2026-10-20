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
