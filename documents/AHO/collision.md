# 2D Collision

`<AHO/core/collision.hpp>`は2DのTriangleとPolygon、同じgeometry conceptを満たす三角形集合を受け付けます。形状を変更せず、判定ごとに座標を取り出すため、外部オブジェクトの寿命を保持しません。

```cpp
const auto mesh = aho::triangulate(contour).value();
const bool hit_point = aho::collision::contains(mesh, aho::d2::PointF(0.5f, 0.5f));
const bool hit_shape = aho::collision::intersects(mesh, other_mesh);
const bool same = aho::collision::CollisionJudger{}(mesh, other_mesh);
```

- 頂点・辺への接触もtrueになる閉集合として判定します。退化した三角形は線分や点として扱います。
- Polygonは三角形の和集合です。凹形状のくぼみを含む外接矩形だけでは判定しません。三角形の重なりや共有辺も扱います。
- 座標情報CIが一致する形状同士で使用し、スカラー型は混在できます。3D・異なる座標情報・スイープ判定・接触深度/法線の計算は対象外です。
- 空のメッシュはcontains/intersectsともfalseです。NaN/Infinityはstd::invalid_argumentで拒否します。
- 数値判定はtriangulateと同じlong doubleへの変換と共通スケール、演算項に応じた許容値を使います。厳密述語ではなく、極端な縦横比や変換時に精度を失う整数について数学的な厳密性は保証しません。
- containsはO(T)、intersectsはO(T₁×T₂)。三角形ごとのAABBで早期除外し、辺交差と包含を判定します。BVHや空間分割は別の最適化対象です。
