# experimental Mesh

`aho::x::Mesh<PolygonType, VertexData, TriangleData>`は、既存Polygonに頂点属性と三角形属性を対応づけるCPU側の型です。属性はsaitekikuukanのSoAへ分離され、GPU転送に渡せる列spanを取得できます。

使い方は[コンパイル可能なサンプル](../../AHOLi/AHO/examples/mesh.cpp)を参照してください。生成にはPolygonだけを渡して属性を既定値で作る方法と、属性のspanも渡してコピーする方法があります。後者は頂点数・三角形数との一致を検証します。

```sh
cmake -S . -B build -DAHO_ENABLE_EXPERIMENTAL_MESH=ON -DAHO_BUILD_MESH_EXAMPLE=ON
cmake --build build --target aho-mesh-example
./build/AHOLi/aho-mesh-example
```

GCC 16の`-freflection`が利用可能な場合、`VertexData`と`TriangleData`のメンバー登録は不要です。CMakeがsaitekikuukanの`record_soa`を実際にコンパイルして判定し、`AHO::ExperimentalMesh`から必要なオプションを伝えます。reflectionを必須にする場合は次のように構成してください。

```sh
cmake -S . -B build/reflection \
  -DCMAKE_C_COMPILER=gcc-16 -DCMAKE_CXX_COMPILER=g++-16 \
  -DAHO_ENABLE_EXPERIMENTAL_MESH=ON -DAHO_REQUIRE_MESH_REFLECTION=ON \
  -DAHO_BUILD_MESH_EXAMPLE=ON
```

GTest等のC++ライブラリも同じコンパイラー・標準ライブラリでビルドしたものを使います。既存のClang/libc++用buildディレクトリは流用しません。CIの`Mesh C++26 reflection`はGTestもGCCでビルドし、reflection必須の構成でMeshとsaitekikuukanのreflectionテストを実行します。

Polygonを含むAHOの通常ビルドで、saitekikuukan masterのcommit `76cd8b6052fdc57514b673b15928d9fb5c2f96a9`を取得します。開発中は以下を追加し、ローカルの変更を使えます。

```sh
-DFETCHCONTENT_SOURCE_DIR_SAITEKIKUUKAN=/path/to/saitekikuukan
```

利用するCMake targetは`AHO::ExperimentalMesh`です。Mesh自体は既定で無効ですが、Polygonもsaitekikuukanを使うため依存は通常ビルドでも取得します。MSBuildでは`SAITEKIKUUKAN_HOME`を取得したsaitekikuukanのルートへ設定してください。AHO/AHOLiのプロジェクトがそのincludeを追加します。AHOの既存install/exportにexperimental targetを登録する対応はまだ行っていません。

- Polygonは位置・三角形indexを所有し、Meshは別途頂点/三角形の属性を所有します。位置をVertexDataへ重複保持する必要はありません。
- 形状と属性の行数は構築後に固定し、属性の列または行proxy経由で値を書き換えます。`vertex_data()/triangle_data()`はconst所有アクセサーです。
- `vertex_column<&VertexData::uv0>()`、`triangle_column<&TriangleData::material>()`はactive sizeのspanを返します。const Meshからはconst要素になります。
- `vertex(i)/triangle(i)`はcheckedな行proxyです。read/writeで構造体へ出し入れし、get<Member>()で属性を参照します。
- Meshのcopyは独立したデータを作り、move後の元Meshは空になります。代入・swapは形状と属性を一緒に更新します。既存view/proxyを所有者の代入・move・swap後まで保持しないでください。
- 同じ位置でもUVや法線が異なる頂点を保持できます。自動で頂点を統合しません。
- 既定の属性storageは`aho::x::MeshAttributes`です。reflection対応時はsaitekikuukanの`record_soa<Row>`で全属性を自動的に列へ分離し、構造体の読み書きも同ライブラリへ委譲します。`column<&Row::field>()`の列番号解決はconstevalで完了し、実行時のメンバー探索はありません。
- reflection非対応時は`MeshAttributeSchema<Row>::Members`へ`MeshAttributeMembers<&Row::field, ...>`を登録します。明示的に列を選ぶ場合は、両環境で`MeshAttributes<Row, MeshAttributeMembers<...>>`を使えます。列の所有にはsaitekikuukanの`soa`を使用します。
- `MeshAttributes`は行数を構築時に決めます。構造体の初期値、同型の複数列、空属性、bool列を扱えます。bool列は連続spanではなくsaitekikuukanのビューを返します。
- 属性storageは第4/第5template引数で差し替え可能です。ValueType、列/行アクセス、要素数指定/行spanによる構築とnothrow swapの契約を満たす必要があります。AoS storageはまだありません。
- 列のメモリー表現とGPUのformat/strideが一致することは転送側が検証します。三角形属性は通常の頂点入力と異なる単位なので、用途に応じた転送・draw範囲が必要です。
- この変更はglTFローダーの型置き換えや描画器を追加しません。点/線primitiveまで三角形Polygonへ変換することもありません。

## Polygonのストレージ

位置と三角形をそれぞれ1列のIDなしSoAで所有します。`points()`は`span<const PointType>`、`indices()`は`span<const IndexedTriangle<Size>>`のままで、取得時のコピーや中間配列は作りません。XYZや三角形の3頂点indexを別列へ分解する変更ではありません。

`Polygon`の従来の型引数と`Alloc`を維持し、三角形列にallocatorを適用します。コピーは独立したデータを作り、移動後の元Polygonは空になります。allocatorの交換が許されない場合は、代入先のallocatorを保って三角形を移します。

## ツールチェーンとストレージの制約

AHOの言語モードはC++26です。自動展開は`__cpp_impl_reflection >= 202506L`を必要とし、GCC 16で検証しています。LLVM 20の規約検査と互換構成では明示的な登録を使います。`AHO_REQUIRE_MESH_REFLECTION=ON`でreflectionを利用できない場合は、configure時に失敗します。

自動展開できるstructの制約はsaitekikuukanの`record_soa`に従います。列に分割できる通常のデータメンバーを持つstructを使用してください。独自の`MeshAttributeSchema`をreflection対応時にも適用する場合は、`MeshAttributes<Row, typename MeshAttributeSchema<Row>::Members>`と明示します。異なるreflectionオプションで同じMesh型を翻訳単位間に共有しないでください。

ビルド要件は[言語標準の説明](../development/cxx-standard.md)を参照してください。

汎用のallocator付き列でcopy/swapを使う場合も、allocatorの同一性やpropagate規約を満たす必要があります。Polygonではこの条件を判定して対応しています。
