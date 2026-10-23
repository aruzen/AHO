# C++の規約検査

AHO/VSLの整形・命名・宣言/実装の配置を検査します。検査はソースを書き換えません。LLVMのメジャーバージョンは20で統一します。

## 実行

通常のビルド依存に加えて、LLVM 20のclang-format・clang-tidy・Clang/LLVM開発ライブラリとPython 3.10以上が必要です。macOSでは`brew install llvm@20`で用意できます。Vulkan SDK等の通常のヘッダー検索設定も有効にしてください。

```sh
export PATH="$(brew --prefix llvm@20)/bin:$PATH"
cmake -S . -B build/style \
  -DAHO_HOME="$PWD/AHOLi/AHO" -DVSL_HOME="$PWD/VSLi/VSL" \
  -DBUILD_TESTING=ON -DAHO_STYLE_CHECKS=ON -DCMAKE_BUILD_TYPE=Release \
  -DAHO_ENABLE_EXPERIMENTAL_MESH=ON -DAHO_BUILD_MESH_EXAMPLE=ON \
  -DLLVM_DIR="$(brew --prefix llvm@20)/lib/cmake/llvm" \
  -DClang_DIR="$(brew --prefix llvm@20)/lib/cmake/clang"
cmake --build build/style --target aho-style-ast
ctest --test-dir build/style -R '^Style\.' --output-on-failure
cmake --build build/style --target check-style
```

`AHO_STYLE_CHECKS`は既定でOFFです。検査ツールを使わない通常ビルドにLLVM開発ライブラリは不要です。ONの場合は`compile_commands.json`も生成します。製品やGPUテストのビルド・実行は検査の前提にしていません。

整形だけなら通常ビルドのconfigureも不要です。

```sh
python3 tools/style/check.py --checks=format
```

全違反を失敗として扱う場合は`--strict`を付けます。通常実行では追跡済みの`tools/style/baseline.json`と比較し、新しい違反があれば終了コード1、コンパイラの解析失敗や設定不備では2を返します。JSONとTUごとのログは`build/style-report/`へ出力します。

末尾へファイル名を指定すると対象を絞れます。ソースだけを指定した場合はそのソースのコンパイル条件をすべて検査し、ヘッダーを指定した場合は漏れを避けるため全TUを読みます。

クラス内定義は、本体のコード行が5行以上の場合に分離を必須とします。4行以下はクラス内定義を許可します。宣言・外側の波括弧・空行・コメント・通常のコンストラクタ初期化子リストは数えません。内部のブロックの波括弧や文字列リテラルはコードとして数えます。長い初期化子リストや、短くても複雑な処理はレビューで分離を判断します。

閾値はCLIの`--in-class-min-lines`、CMakeの`AHO_STYLE_IN_CLASS_MIN_LINES`で変更できます。どちらも既定値は5です。0を指定すると全件を検査します。function-try-block等、行数を確実に取得できない特殊な本体は診断に残します。

条件変更の影響を調べる際は`--strict`で既存のbaselineを使わずに集計してください。条件を変える際にはbaselineも見直し、短い関数の旧許可が、後から本体を増やした際にも残らないようにします。

## 検査する規則

| 対象 | 方法 |
| --- | --- |
| 空白・インデント・改行等 | `.clang-format`とLLVM 20の出力を比較 |
| 型名 | PascalCase |
| staticメンバー関数 | PascalCase |
| その他の関数・namespace・通常の変数 | snake_case |
| privateの非staticメンバー変数 | `_snake_case` |
| static/global変数・enum要素 | UPPER_SNAKE_CASE |
| 構造化束縛 | 通常の変数と同じ規則。AST検査で補完 |
| 型・staticメンバー関数中のGPU/CPU/VSL/AHO | 略語を小文字化していないかASTで検査 |
| クラス内の関数本体 | 5コード行以上は分離必須。4行以下はクラス内定義可 |
| ヘッダー内の通常関数の定義 | inline・template等を除き検出 |
| 宣言と定義の順序 | 同じファイル・namespace内で、関数定義の後に型定義/関数宣言がある場合を検出 |

命名は`.clang-tidy`の`readability-identifier-naming`を使用し、配置は`tools/style/ast_check.cpp`で検査します。`const`/`constexpr`だけでは通常変数を大文字にしません。ファイルスコープのstatic関数もsnake_caseです。

`= default`・`= delete`、ラムダ、ローカルクラスの配置は除外します。STLの型エイリアスとGTestのPrintToなど、外部仕様で要求される名前は設定で区別します。仮想関数のoverride名は基底の契約に従います。

衝突回避用の末尾`_`は、その前が命名規則に適合する場合だけ許可します。衝突が実在するかの判定はレビューで確認します。

対象はGitで追跡された`AHOLi/AHO/`・`VSLi/VSL/`・`tools/style/`内のC/C++です。整形は全対象、ASTとclang-tidyはコンパイルDBの全対象TUを走査し、その中で読まれたヘッダーも検査します。同じファイルでもGPU等のコンパイル定義が異なるコマンドはそれぞれ解析します。DB外のソースと未解析のヘッダーも診断し、今回の既存分はbaselineに記録します。新たに追加するファイルはDBまたはヘッダーを読むTUへ登録してください。未選択の`#if`分岐や未展開マクロの内部はAST検査の対象になりません。

`tools/style/headers.cpp`は公開umbrellaを読み、通常のTUが使わない予約ヘッダーも解析対象へ含めます。このために製品のリンクやアプリの起動は行いません。

## 既存違反の扱い

初期導入時の既存違反をbaselineに記録し、規約違反そのものは残件として見える状態にします。規則の例外として無条件に許可するファイル一覧ではありません。

5コード行の条件では、baselineは224件（クラス内定義47・宣言順128・整形49）です。短い関数379件の旧許可は削除しています。

命名・配置ではファイル・規則・識別子・診断行の内容と件数を照合します。同じ違反の複製や診断行の変更も新規違反になります。整形と未解析ファイルはファイル内容のハッシュを照合するため、既存違反のあるファイルを変更する際は全体の整形や解析対象への登録が必要です。

baselineの更新は全対象・全検査が完了した場合だけ可能です。CIでは更新しません。違反を追加したままbaselineを更新して通す運用は避け、差分の理由をレビューしてください。

```sh
python3 tools/style/check.py --database build/style \
  --ast-tool build/style/tools/style/aho-style-ast --write-baseline
```

## 検査器の検証

`Style.CheckerContracts`は小さな正常例と違反例をコンパイラに入力します。static/非static、const/constexpr、外部契約、配置の例外、namespace再開、構造化束縛、解析エラー、baselineを利用した違反の複製を確認します。

`.github/workflows/style.yml`ではmacOSとLLVM 20で同じ検証・全体検査を実行し、診断をartifactへ保存します。Windows/Linux固有の分岐を網羅するものではありません。
