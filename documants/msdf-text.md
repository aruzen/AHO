# MSDF文字リソース

`AHO::MSDF` は、msdfgenによるフォントの字形生成と、AHOのMesh／Bufferによる文字レイアウトを提供します。実験的APIは `AHO/x/resource/msdf_font.hpp` と `AHO/x/text/text_layout.hpp` にあります。ウィンドウやGPUの初期化なしで利用できます。

```cpp
#include <AHO/x/text/text_layout.hpp>

aho::x::resource::FontFace face("font.ttf");
aho::x::resource::MSDFAtlas atlas(face);
auto text = aho::x::text::layout_text(
    atlas, u8"こんにちは AHO", {.baseline = {20, 80}, .em_size = 32});
auto buffers = text.make_buffers();
auto updates = atlas.updates_since(0);
```

## ビルド

通常のAHOの依存に加えてFreeTypeの開発用パッケージを用意し、CMakeに `-DAHO_ENABLE_MSDF=ON` を指定します。msdfgenは固定コミット `1c106ed8117893bf943e577f62eb0665fb271e46` から取得します。オフライン環境では `-DFETCHCONTENT_SOURCE_DIR_MSDFGEN=/path/to/msdfgen` を指定できます。既定はOFFです。

利用側のターゲットは `AHO::MSDF` にリンクします。msdfgen／FreeTypeの型を公開APIへ持ち出す必要はありません。`-DAHO_BUILD_MSDF_EXAMPLE=ON` で、生成したデータの件数を表示する `aho-msdf-example` もビルドできます。

```sh
cmake --build build --target aho-msdf-test aho-msdf-example
ctest --test-dir build -R '^AHO[.]MSDF[.]' --output-on-failure
./build/AHOLi/aho-msdf-example path/to/font.ttf '日本語 AHO'
```

## フォントとアトラス

- `FontFace` はファイルまたは `span<const byte>` を読み込み、元データを所有します。コピーはフォントを共有します。現在はフォントコレクションの先頭faceを使用します。
- `metrics()`、字形のadvance／ink bounds／plane bounds、`kerning()` の単位はemです。フォント座標のY軸は上向きです。
- `MSDFAtlas::glyph()` は必要な字形を遅延生成し、glyph index単位でキャッシュします。`prepare()` は文字列に必要な字形を先に生成します。
- `pixels_per_em` はアトラス生成解像度、`pixel_range` は距離の全幅です。レイアウトの `em_size` と独立して設定できます。
- ページは固定サイズで字形を追記します。既存のpage番号・UV・矩形は変化しません。ページを再配置・縮小・破棄する処理はありません。最大ページ数を超えた場合や字形が1ページより大きい場合は `length_error` です。
- 固定ページの簡単な棚詰めを使用しています。msdf-atlas-genの動的アトラスが行う拡張・再配置を避け、既存MeshのUVを安定させるためです。詰め方の最適化は行っていません。
- 存在しない文字は既定でU+FFFD、なければ `?`、それもなければglyph 0へ置換します。返る字形の `fallback` で判別できます。`MissingGlyphPolicy::ERROR_` なら例外になります。
- 空白など輪郭のない字形はadvanceを持ち、ページを消費しません。`drawable()` はfalseです。

`page(i)` は幅・高さ・線形RGBA8の全画素Buffer・更新revisionを返します。画素は上の行から、各行は `width * 4` byteで格納します。`rectangle` の原点は左上、UVは矩形の外縁を幅・高さで割った値です。plane boundsは距離場の余白を含み、ink boundsは実際の輪郭だけを表します。

RGBがMSDFの距離、字形矩形内のAは255です。背景の距離は負、内部は正、境界は0.5です。RGBの中央値から距離を復元し、sRGB変換を適用しない形式で利用してください。`unit_range()` は `{pixel_range / width, pixel_range / height}` を返します。GPUの画像・Sampler・shader・描画Pipelineは利用側で構築します。

## 差分と寿命

`updates_since(revision)` は、そのrevisionの**後**に追加された字形の更新一覧を返します。各更新はpage番号、矩形、revision、矩形内だけを隙間なく格納したRGBA8のGENERAL Bufferを持ちます。全ページの再転送を避けるための情報です。新しいページはゼロクリアした状態から更新を反映するか、`page(i).pixels` を全転送してください。revisionはアトラスごとに管理します。

更新一覧はスナップショットで、返ったBufferはアトラス破棄後も有効です。既存AHO Bufferと同様にコピーは保存領域を共有するため、受け取ったアトラス画素・更新Bufferは読み取り用として扱ってください。全ページBufferは後続の字形追加を反映します。更新履歴はアトラスの寿命中保持されます。

`MSDFAtlas` のコピーと `TextLayout` はキャッシュを共有します。`TextLayout` を保持している間はそのフォント・アトラスも生存します。同じアトラスへの字形追加・レイアウト・画素の読み取りは呼び出し側で直列化してください。独立したアトラスが同じFontFaceを使う場合のFreeTypeアクセスは内部で同期します。

## レイアウトと転送データ

`layout_text()` はUTF-8の横書きを行います。不正なUTF-8は `invalid_argument` です。CRLF／CR／LF、タブ、カーニング、字間に対応します。字間は同じ行の通常字形の間に入り、行末には加算しません。高度なshaping、合字、双方向組版、縦書き、自動折り返し、別フォントへのfallbackは未対応です。kerningはFreeTypeが取得できるペア情報を使用します。

`TextOptions` のbaseline、em_size、letter_spacingは利用側の座標単位です。line_spacingはフォントline heightに掛ける倍率です。既定のY軸は下向きで、`ClipY::UP` も選べます。三角形の符号付き面積はどちらでも数値座標上で正です。最終的な投影・viewportを含む表裏判定は利用側の設定に合わせてください。

- `mesh()` はPolygonを幾何として持つ既存の `x::Mesh` です。位置・UV列と三角形ごとのpage列を取得できます。C++26 reflectionを要求しない明示的な列定義を使用しています。
- `make_buffers()` は位置float2＋UV float2の `TextVertex` 列とuint32のindex列を、それぞれAHO `Buffer<GENERAL>` に格納します。レイアウト変更時に一度作り、既存のBuffer転送APIへ渡せます。indexの順序はMeshと一致します。
- `ranges()` はpage番号と連続したindex範囲（byte単位ではありません）を返します。文字の重なり順を保つため、page別の並べ替えは行いません。
- `glyphs()` はコードポイント・元UTF-8のbyte offset・baseline・advance・字形情報を返します。空白／タブも含み、改行は含みません。
- `metrics()` のminimum／maximumは余白を除いたink bounds、widthは各行の最大論理advance、heightは行数×行送り、endは最後のpen位置です。空文字列は0行、空白だけの文字列は幾何なしで論理幅を持ちます。

一次資料: [msdfgen](https://github.com/Chlumsky/msdfgen)、[msdf-atlas-gen DynamicAtlas](https://github.com/Chlumsky/msdf-atlas-gen/blob/master/msdf-atlas-gen/DynamicAtlas.h)。
