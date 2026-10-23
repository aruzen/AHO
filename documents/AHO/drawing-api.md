# 2D描画API（experimental）

`<AHO/x/drawer/draw_list.hpp>`の`aho::x::DrawList`はCPU上の描画リストです。VulkanやGPUに依存せず、頂点を所有し、読み取り専用spanで公開します。

```cpp
aho::x::DrawList list;
list.reserve(1024);
list.rectangle({10, 20, 100, 50}, aho::RGBA(1, 0, 0, 1));
list.set_transform({.xx = 2, .yy = 2, .x = 10, .y = 20});
list.triangle(triangle, aho::RGBA(0, 1, 0, 1));
// list.geometry(mesh, color)は同じ座標情報を持つfloat/2Dのtriangles()を受け付ける。
```

triangle/rectangle/geometryは呼び出した時点のTransform2Dを適用します。変換は2×2行列と平行移動です。clear()は容量を維持して内容と変換を初期化します。コピーは独立した頂点列です。点・色・UV・変換の非有限値とuint32_tの頂点数上限を検査し、失敗時は以前の描画内容を維持します。色は有限値のみを要求し、[0,1]への制限は行いません。

rectangleの負の幅・高さを許可します。UVも符号付きの矩形なので反転できます。TextureId{0}は単色、その他はRendererに登録するIDです。同じIDが連続すると1バッチにまとめます。色が違っても頂点属性で保持するため統合できます。透明な図形の重なり順を維持するため、ソートや離れたバッチの統合は行いません。

DrawListとそのspanはスレッド間で共有しながら変更できません。spanは次の変更・再代入・move・破棄で無効になる場合があります。geometry()は一時リストで検証してから追加します。実験的APIなので、既存の暗黙draw()/pack()の動作は変更しません。
