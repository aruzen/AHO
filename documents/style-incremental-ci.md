# style CIの差分検査化

通常のPRとpushで全TUを解析していたため、Sampleの小さな追加にも全体検査の時間が掛かっていた。通常は変更ファイル、GitHub Releaseのpublished時は全体を検査するように変更した。Sampleとは独立したPRとする。

## 検査範囲

- PR: base/headの共通祖先からheadまで。base branchの前進をPRの変更に混ぜない。
- masterへのpush: push前commitからHEADまで。まとめてpushした複数commitを含める。
- ローカル: index・作業ツリー・未追跡の追加変更。削除は検査せず、rename先を検査する。
- Release: 全対象。手動実行は既定が直前commitとの差分で、base_refとfullを明示指定できる。

CIはcleanなcheckoutなのでgit statusには依存しない。変更対象がなければ依存インストールから省略し、検査器・規約設定だけの変更はstandaloneのchecker contractを実行する。通常のsource変更で毎回contractを実行することも省いた。

## ヘッダーと失敗条件

変更ヘッダーはコンパイラーの`-M`で依存するTUを調べてから、そのTUだけをAST/命名検査する。単純なinclude文字列検索にせず、macro・条件分岐・推移的includeと複数のコンパイル条件を維持した。無関係なTUは前処理だけで、ASTを作らない。共通ヘッダーの変更は対象TUが多くなる。

DB未登録の変更source、どのTUからも読まれない変更header、依存取得の失敗は引き続き失敗にする。差分検査でも変更ファイル全体を検査し、既存のmaster照合を使う。規則・baselineの意味は変更しない。

CIの基点・選択ファイルはbuild/style-scope.json、検査結果はbuild/style-report/report.jsonに保存する。全体検査はcheck-style-full、ローカル差分はcheck-style、commit差分はAHO_STYLE_BASE_REFで選べる。CLIの引数なし実行は既存互換の全体検査を維持する。

## 検証

LLVM 20/C++26でcheckerをbuildし、既存25件と新規14件、計39件のcontractが成功。PRの基点前進、push範囲、初回commit、rename/削除/空白path、未コミット変更、条件付きmacro include、全コンパイル条件、無関係な壊れたsourceの除外、依存解析失敗、DB未登録の拒否、差分/fullのCMake targetを確認した。workflowのYAML・shell構文も確認した。

実際のTyped Sampleの3ファイル（headerを含む）を選ぶと、既存DBの222TUからAST対象は2TUになった。残る220TUの依存取得は手元で46.22秒。sourceだけの変更では依存取得も不要。これは依存取得の実測であり、GitHub CI全体の所要時間ではない。
