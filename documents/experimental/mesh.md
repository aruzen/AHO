# experimental Mesh

`aho::x::Mesh<PolygonType, VertexData, TriangleData>`は、既存Polygonに頂点属性と三角形属性を対応づけるCPU側の型です。属性はsaitekikuukanのSoAへ分離され、GPU転送に渡せる列spanを取得できます。

使い方は[コンパイル可能なサンプル](../../AHOLi/AHO/examples/mesh.cpp)を参照してください。生成にはPolygonだけを渡して属性を既定値で作る方法と、属性のspanも渡してコピーする方法があります。後者は頂点数・三角形数との一致を検証します。

```sh
cmake -S . -B build -DAHO_ENABLE_EXPERIMENTAL_MESH=ON -DAHO_BUILD_MESH_EXAMPLE=ON
cmake --build build --target aho-mesh-example
./build/AHOLi/aho-mesh-example
```

通常は固定commitのsaitekikuukanを取得します。開発中は以下を追加し、ローカルの変更を使えます。

```sh
-DFETCHCONTENT_SOURCE_DIR_SAITEKIKUUKAN=/path/to/saitekikuukan
```

利用するCMake targetは`AHO::ExperimentalMesh`です。この機能は既定で無効で、その場合saitekikuukanは取得しません。MSBuild等からヘッダーを使う場合は、AHO/VSLのincludeに加えて対応するsaitekikuukanのincludeを設定してください。AHOの既存install/exportにexperimental targetを登録する対応はまだ行っていません。

- Polygonは位置・三角形indexを所有し、Meshは別途頂点/三角形の属性を所有します。位置をVertexDataへ重複保持する必要はありません。
- 形状と属性の行数は構築後に固定し、属性の列または行proxy経由で値を書き換えます。`vertex_data()/triangle_data()`はconst所有アクセサーです。
- `vertex_column<&VertexData::uv0>()`、`triangle_column<&TriangleData::material>()`はactive sizeのspanを返します。const Meshからはconst要素になります。
- `vertex(i)/triangle(i)`はcheckedな行proxyです。read/writeで構造体へ出し入れし、get<Member>()で属性を参照します。
- Meshのcopyは独立したデータを作り、move後の元Meshは空になります。代入・swapは形状と属性を一緒に更新します。既存view/proxyを所有者の代入・move・swap後まで保持しないでください。
- 同じ位置でもUVや法線が異なる頂点を保持できます。自動で頂点を統合しません。
- 属性storageは第4/第5template引数で差し替え可能です。ValueType、列/行アクセス、要素数指定/行spanによる構築とnothrow swapの契約を満たす必要があります。AoS storageはまだありません。
- 列のメモリー表現とGPUのformat/strideが一致することは転送側が検証します。三角形属性は通常の頂点入力と異なる単位なので、用途に応じた転送・draw範囲が必要です。
- この変更はglTFローダーの型置き換えや描画器を追加しません。点/線primitiveまで三角形Polygonへ変換することもありません。
