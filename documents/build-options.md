# ビルドオプション

AHO/VSLはCMake・ツールチェーン・利用者が指定した`CMAKE_CXX_FLAGS`や構成別フラグを上書きしません。デバッグ情報と診断の追加設定は、AHO、AHOLi、VSLの各targetにPRIVATEで適用します。依存ライブラリやリンクする利用側には伝播しません。

`AHO_NATIVE_OPTIMIZATION`は既定で`OFF`です。ビルドしたマシン専用の実行ファイルを作る場合は、GCC/Clangで`-DAHO_NATIVE_OPTIMIZATION=ON`を指定できます。Release/RelWithDebInfoの上記targetに`-march=native`を追加します。別CPUへ配布する場合は既定値を使ってください。

最適化レベルはCMakeの標準設定または利用者のツールチェーンで選びます。AHOが独自に`-O3`を重ねることはありません。
