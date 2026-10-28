# AHOの公開ヘッダーとVSL

AHOのヘッダーは`VSL_ENABLE_SPIRV_REFLECTOR`を定義しません。`engine.hpp`や`draw_phase.hpp`は自身が使用するVSLの個別公開ヘッダーを読み込みます。

SPIR-V解析を使うコードでは`#include <VSL/utils/spir_reflector.hpp>`を直接記述します。VSLのumbrella headerを先に読んでいても、同じAPIを利用できます。pipeline stageやcommandを直接作るコードでも、その型を定義するヘッダーを明示します。

従来AHOヘッダーから間接的に見えていたVSLの型が必要な利用側は、対応するincludeを追加してください。ライブラリの機能設定を変えるためにAHOヘッダーを先に読む必要はありません。
