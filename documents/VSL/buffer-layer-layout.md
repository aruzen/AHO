# Buffer refactorのディレクトリと保持契約

2026-11-06。PR #317の追加レビューと、利用者が承認したディレクトリ構成を反映する。[世代・アクセス・識別・寿命群](buffer-generation-contracts.md)を前提とする。

## 4責務の配置

`VSLi/VSL/include/VSL/`、`src/`、`src/private/VSL/`を、次の4分類へ揃える。

| ディレクトリ | 責務 | 主な配置 |
| --- | --- | --- |
| storage | CPU領域の確保・使用権・寸法・offset | ArenaMemory、CPURegion、BufferLayout、ABI定数 |
| lifetime | 構築・rollback・所有・資源保持・破棄 | BufferArena、BufferView、TypedBufferArena、ResourceUse／Group、buffer_storage、submission_retention |
| sync | 使用状態・アクセス可否・完了観測 | ResourceUseState、CompletionObservation、Completion、barrier方針、submission最終完了 |
| operation | 解決済みImplへの具体的なVulkan Buffer操作 | BufferCreateInfo、BufferCopyVisibility、buffer_ops |

公開APIは`include/VSL/<分類>/`、実装は`src/<分類>/`、非公開の連携は`src/private/VSL/<分類>/`へ置く。旧backend／contractsディレクトリはoperationへ統合する。ResourceUseStateとResourceUse、Completionの完了観測とretainの実装も分ける。

BufferArenaの正式な名前は`lifetime::BufferArena`。既存の`storage::BufferArena`は型aliasで維持する。`VSL/storage/buffer_arena.hpp`、`VSL/utils/typed_buffer_arena.hpp`、`VSL/vulkan/buffer_view.hpp`、`buffer_create_info.hpp`、`resource_use.hpp`は転送ヘッダーとして残す。公開クラスを二重定義しない。静的ライブラリとconsumerは一緒に再ビルドする。

既存の通常Buffer・command・Phaseは4責務を組み合わせる公開facadeなので、既存のvulkan配下を維持する。全Vulkan資源を同時に移行する変更ではない。Implの旧集合定義は引き続きprivateのvulkan/_pimpls.hppにある。

## Typed入口の借用

TypedBufferArena::storage()はconst lvalueだけに許可する。rvalue／const rvalueからの呼出しをdeleteし、下位のborrow()の制限を迂回できないようにする。

```cpp
Arena arena(device, plan);
auto ref = arena.storage().borrow(0); // arenaが生存している間だけ使える借用
auto owned = Arena(device, plan).view<SOURCE>(); // owning viewの取得は有効

// コンパイル失敗。一時arenaの破棄後に借用を残さない。
auto dangling = Arena(device, plan).storage().borrow(0);
```

下位BufferArenaだけの負例に加え、Typedの一時値とconst rvalueを使う2件をcompile契約へ追加する。const lvalueのborrowと、一時値からのowning viewは正常例で確認する。

## 使用登録とowner保持

ownerの保持はlifetime::retain_resourceへ集約する。CompletionとCommandBufferの保持APIは、同じcontrol blockを一つの保持一覧へ一回だけ登録する。格納pointerの一致で判定しない。異なるslotを指すaliasやnull aliasは同じownerになり、同じaddressを持つ別control blockは別ownerとして保持する。

const参照の入力は重複確認後だけコピーし、rvalueは保持一覧へmoveする。頂点／index bindではResourceUse::recordが状態登録と保持を担当し、その直後の二重retainを削除する。

descriptorの物理Bufferは、現在のnative binding表を読むrecord_descriptor_buffersが一度だけ登録・保持する。TypedのResourceUseGroup::ForDescriptorも同じ処理を呼ぶ。Typed側のbinding一覧には、Bufferについては追加の論理制約とCustom所有者だけを保存する。画像・samplerの従来の追加使用状態は維持する。TypedBatchのbind省略時も、同じtrack入口を通す。

複数setを持つ通常VSLのbindでは、Typedのgroupが登録済みのsetだけを省略する。残りのset、および通常のResourceUseGroupを渡した場合は、それぞれのnative binding表から資源を登録する。

AHO由来の追加論理制約を差し替える場合はTypedのwriteを通す。native updateだけでAHOの論理資源を推測することはしない。

groupが所有するdescriptor実体へのポインターは借用で、既存の_ownerが実体を保持する。arenaの内部実体にarena自身への強い所有は作らない。

保持をまとめても、使用状態は実体ごとに登録する。同じarenaを所有する二つのBufferImplには二つの使用状態があり、ownerが既に登録済みでも二つ目の状態登録を省略しない。

旧resourcesと、新しいcompletion_resourcesは解放契約が異なるため、一覧ごとに重複排除する。両一覧を横断してまとめ、完了直後の解放や旧Completionの保持保証を変えることはしない。依存する別ownerを、同じaddressや親子関係だけで省略することもない。

## レイアウト生成の対応環境は別の判断

今回もCMakeの寸法生成は必須で、VSL全体に単一arch・対応compiler等の制約がかかる。静的な正確な数値とartifact guardを保つ方針は維持した。生成処理は移動後のcanonicalなsync／lifetimeヘッダーをhash・configure依存に含め、転送ヘッダーだけのhashで整合性を判定しない。

通常VSLからこの制約を切り離すなら、次の変更を別の設計判断として行う。

1. 共通のoperation・buffer_storage・heap Bufferは通常VSLに残す。ここは実定義のsizeof／alignofを直接使い、生成ヘッダーを必要としない。
2. 寸法生成、buffer_layout.cpp、lifetime/buffer_arena.cppを配置機能の条件付きtargetへ分ける。
3. 配置機能を要求したconsumerだけに、生成ヘッダーとABI選択定義を伝播する。
4. installで配置機能の有無を明示し、要求されたのに利用できない場合はconfigure時に拒否する。

既定をopt-inにするか、機能の無効化を選べる形にするか、target／package componentの名前は未決。今回は4層とAPI契約の整理を進め、通常VSLの対応環境を変更したとは扱わない。

## 今回の検証

macOS arm64／Apple M1、Clang20、ASan／UBSan、MoltenVK／Vulkan validationで確認した。

| 検証 | 結果 |
| --- | --- |
| VSL全体 | 698件中695成功、対応外packed formatの3件skip。追加のowner重複・別slot状態・native descriptor更新・複数set保持・alias保持も成功 |
| AHO回帰 | GPU121件・CPU1070件成功 |
| Typedサンプル | 3 frame slot／12 submission／2 draw／UBO更新／resizeを確認。pipeline生成は1回 |
| Typed compile契約 | VSL121件＋AHO TypedBuffer9件の130件成功。arenaはTypedの一時借用拒否2件を含む15件 |
| 規約 | 変更C++41ファイル／24TUを検査し、未解析の転送ヘッダー2枚を4ファイル／8TUの追加strict検査で補完。コード違反・未解析の残りは0 |
| install | canonicalと旧includeを同時に使うconsumerのbuild／link／CPU検証成功。private Implは非公開のまま |
| 寸法とartifact | Debug／Releaseの実寸法assert、x86_64向けobject生成に成功。寸法が同じでも異なるartifact情報とbinaryの混在はlink guardで拒否 |
| build設定 | CMakeの関連targetをbuild。MSBuild XML、今回の移動先、重複登録を検査。Windows実行は未検証 |

AHOの既存compile検査まで広げた231件では227件成功、Polygon結合の4件が失敗した。`Line.PolygonAddition`、`PtrLine.PolygonAddition`、`Triangle.POLYGON_ADDITION`、`PtrTriangle.POLYGON_ADDITION`で、Polygonの不完全型への演算が原因。変更前の`5961a5fd`のヘッダーとテストを取り出して同じ4件をコンパイルし、今回と完全に同じ診断を確認した。このrefactorの修正対象には含めていない。

Linux／Windows実機、x86_64での実行、並行CPUアクセス、device lost、性能比較は未検証。LeakSanitizerは無効化し、既存のGPU完了／取消テストとweak_ptr・deleterによる解放検査を用いた。
