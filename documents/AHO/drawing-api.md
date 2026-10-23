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

## Renderer2D

`<AHO/x/drawer/renderer2d.hpp>`のRenderer2Dは、device・RenderPass・SPIR-Vのディレクトリから作成します。パスやデバイスにグローバルな既定値を設けません。CMakeのAHOLiビルドでglslcを使って`build/AHOLi/shaders/drawer/`へシェーダーを生成し、installでは`share/AHO/shaders/drawer/`へ配置します。MSBuildはVULKAN_SDKのglslc.exeで`$(OutDir)shaders/drawer/`へ生成します。

```cpp
aho::x::Renderer2D renderer(device, render_pass, shader_directory); // ループ外で再利用
// DrawPhaseなら既定のRenderPassが開始済み。ComputePhaseではBegin/Endを明示する。
if (!renderer.record(phase, list, width, height)) { /* 空きスロット不足 */ }
// 描画ストリームへ明示的に渡す場合:
if (auto command = renderer.draw(phase, list, width, height))
    phase << *command;
```

record()は記録を行ってbool、draw()は未記録のスナップショットをoptional<aho::x::command::Draw2DCommand>で返します。draw()の戻り値を使う場合は`<AHO/x/command/draw2d.hpp>`もincludeしてください。draw()の時点で頂点をGPUバッファへコピーするので、元のDrawListは直後に変更・破棄できます。コマンドは作成したPhase内だけで記録できます。Phaseは記録中で、Rendererと同じdeviceでなければなりません。RenderPass/subpass/deviceの互換性はRenderer作成時と描画時で一致させてください。対象subpassはRGBA8/BGRA8のUNORMまたはsRGB、sample数1のcolor attachmentが1つ、depth/stencilなしに限定し、それ以外は構築時に拒否します。

座標は左上原点、右向きX・下向きYのピクセル座標です。シェーダーで描画先の幅・高さからNDCへ変換します。幅・高さは1〜2²⁴で、実デバイスのviewport/imageの上限も守る必要があります。Rendererは指定サイズのviewport/scissor、カリングなし・深度なし・通常の非乗算済みalpha blendingを設定します。後続の独自コマンドは必要な状態を再設定してください。

GPU使用中の領域は上書きせず、Completionを弱参照で追跡します。コマンド自体のコピーが残るスロットも再利用しません。空きスロットがない場合はrecord=false / draw=nulloptで、暗黙waitや追加のsubmitはしません。max_snapshotsは既定3で、未使用のコマンドを保持する場合も上限に含みます。完了・cancel後は再利用します。バッチごとに頂点バッファを持ち、容量不足のときだけ作り直します。capacity()は確保済みバッファの合計バイト数です。Rendererのコピーはスロット管理を共有し、同時変更には外部同期が必要です。

Draw2DCommandの`getPipeline()`はVSLのコマンドconceptが要求する名前を維持します。

GPU完了までのネイティブ資源保持はVSLの公開コマンドに委ねます。AHO製品コードはVulkanヘッダー・_pimpls.hpp・vk型に依存せず、コンパイル契約でも禁止します。未登録のTextureIdはdraw/recordの時点で例外にします。

## テクスチャ

```cpp
renderer.set_texture(aho::x::TextureId{1}, image, sampler);
list.rectangle({10, 20, 100, 50}, aho::RGBA(1, 1, 1, 1), {1});
// UVを矩形で指定。負の幅・高さなら反転する。
list.rectangle({120, 20, 100, 50}, aho::RGBA(.5f, 1, 1, 1), {1}, {1, 0, -1, 1});
```

IDはRendererごとの登録番号で、0は単色用に予約します。set_textureはUNORM8/sRGB8のR/RG/RGB/RGBA/BGRA、Sampled用途の画像を受け付けます。samplerとimageはRendererと同じdeviceで作成してください。画像は単層2Dで、記録・実行時にはShaderReadOnlyOptimalで参照可能にする契約です。Rendererはアップロード・画像layout遷移・他のキューとの同期を自動では行いません。

登録時は新しいdescriptorを作ります。同じIDを差し替えたりerase_textureで解除しても、準備済みコマンドが保持するdescriptor・画像・samplerは変わりません。元の画像やRendererを破棄しても、記録したGPU処理に必要な資源はVSLが完了まで保持します。erase_textureは登録が存在した場合だけtrueです。

テクスチャ色と頂点色を乗算し、単色と同じalpha blendingを行います。入出力のsRGB変換は画像formatに従います。UVの範囲外はSamplerのedge modeに従います。単色/テクスチャを切り替えても追加順を維持します。DrawListの各バッチを1回のdrawに対応させます。描画用バッファはHostVisible|HostCoherentが利用可能なメモリを必要とします。

## 実行例

```sh
cmake -S . -B build -DAHO_BUILD_DRAWING_DEMO=ON \
  -DAHO_HOME="$PWD/AHOLi/AHO" -DVSL_HOME="$PWD/VSLi/VSL"
cmake --build build --target aho-drawing-demo
./build/AHOLi/aho-drawing-demo ./build/AHOLi/shaders/drawer
```

`examples/drawing2d.cpp`は色付きの三角形と半透明の四角形を描画します。RenderPassがリサイズで置き換わったらRendererを作り直し、通常のDrawPhaseで表示します。画像を読み込む依存は追加していません。
