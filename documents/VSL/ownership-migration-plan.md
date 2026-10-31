# VSLの所有・配置の段階的移行計画

状態: 2026-10-31の当初計画。最初の移行単位の実装状況・採用APIは[Buffer配置の実装](buffer-placement.md)を参照する。下記の未実施表記は計画策定時点。前提・現行経路・API案は[責務と配置の設計](ownership-and-placement.md)を参照する。各段階を独立してreview・rollbackできる変更に分割する。性能改善を受入条件に混ぜず、正しい責務・寿命を先に確立する。

## 1. 実装開始前の基点確認

調査したcheckoutは`233717c6`、後続機能の補足基点は`8959e5bf`。実装PRの開始時に最新masterを再確認し、ResourceUse系列・bundle／allocation plan・AHO adapter・package整理の採用状態を揃える。古いcheckoutにない機能を「今回新規実装」として重複させない。

PR #307のStoragePolicyはwrapper生成の差し替えであり、今回のbackend分離の前提ではない。採用する場合も、CPU Impl placementを行うfactory／lifetime adapterへ接続する別変更が必要になる。

利用者の作業中変更を含めたcheckoutを勝手に更新しない。実装は専用branch／worktreeで行い、PR作成前にmasterへ追従する。

## 2. 段階と終了条件

| 段階 | 変更内容 | 維持する保証／終了条件 |
| --- | --- | --- |
| M0: 契約固定 | 現行のview・再確保・command保持・descriptor更新時点・取消の回帰ケースを整理。実装済みBのテストを利用 | 旧APIの意味が明文化され、変更する保証と保つ保証を区別できる |
| M1: Buffer一本の縦断 | 共通backend、layout生成、CPU単slot配置、owner／borrow分離、view、CopyBuffer記録、submission保持／取消まで通す | 通常heap Bufferとarena Bufferが同じnative処理を使い、どちらもGPU完了まで生存。失敗・取消で回収 |
| M2: schemaの複数slot | CpuObjectLayoutPlan、scope／frame／array展開、管理表、external import、bundleへのopt-in経路 | sharedは一個、per-frameは枠ごと。型が同じ別slotを混同しない。容量・overflow・空planを扱う |
| M3: AHOとstorage世代 | shared Bufferの現在値、旧view snapshot、Custom依存、reallocateのpublish／rollbackを共通ownerへ接続 | 共有alias・旧世代・内容保持・失敗時の旧値を維持。Custom破棄順とbusy契約を維持 |
| M4: image／sampler | ImageImpl・view・native所有区分・layout状態・転送を分離 | owned imageとswapchain借用imageを区別。layout／visibilityと保持を分離し、既存転送結果を維持 |
| M5: descriptor | layout／pool／setの実体配置と依存DAG、binding保持、記録時snapshot、使用中更新拒否 | bind command作成後・記録前の更新を反映。bind省略時も資源を保持。pool destroyの時期を管理 |
| M6: shader／pipeline | shader module・pipeline layout・pipeline・render pass依存の分離。生成scratchとpersistent stateを区別 | reflectionの意味、push snapshot、subpass・互換resizeを維持。driver cache／dynamic stateをCPU配置と混同しない |
| M7: frame／submission／presentation | frame ownerをarena rootへ集約。in-flight registryとCompletionSignalを分離。最後にswapchain世代 | 古いCompletionの結果維持、frame再利用待ち、cancel時の画像返却、present資源の寿命を維持 |
| M8: 内部容量と最適化 | vector／map等のbounded storage、retention pool、bind plan等を測定して改善 | 明示capacity・超過時の方針・失敗保証を保持。測定なしに高速化を宣言しない |

各段階でdynamic VSLを残し、移行済み実体と未移行の実体を同じcommandから利用可能にする。全資源の同時置換は必要ない。

## 3. 最初の移行単位: Buffer一本

### 3.1 対象

CPU regionに配置する実体はまず**host-visibleなBufferImpl一本**と、そのslot／使用状態／rollbackに必要な管理データとする。device・command manager・fence・転送先Bufferは既存の外部ownerを利用する。GPUメモリは現在と同じ専用割当の方針を保つ。

schemaは一つのowned bufferを持つ最小例。payloadとCPU Implのsizeは別に計算する。内部vector全廃・descriptor arena・pipeline arena・GPUメモリsuballocationはこの単位に含めない。

### 3.2 通す経路

```text
private BufferImplからlayout生成・実size assert
  -> 型付き単slot plan
  -> CPU memory providerがaligned regionを一括確保
  -> VSL構築関数がslotへplacement構築
  -> 共通backendでcreateBuffer / allocate / bind
  -> map / 初期値を書込 / 必要なflush
  -> 部分viewを外部公開（特定世代のref + arena owner）
  -> CopyBuffer commandを作成（外部の既存Bufferへ転送）
  -> Buffer wrapper・view・Typed rootを先に解放
  -> commandが保持しているのでinvoke可能
  -> 記録側へarena pinを移譲／追加
  -> commandも解放
  -> submit / fence完了 / destinationのinvalidate・readback照合
  -> 使用lease・submission pinを解放
  -> 最後のownerでnative破棄・Impl破棄・CPU領域返却
```

転送先を既存Bufferにするのは、初回からdescriptor・pipelineを作り直さずに、新旧所有方式を混在させたGPU寿命契約を検証するため。Bufferが二本存在しても、arena移行の対象は一本である。

別経路として、同じBuffer一本を作った記録をcancelし、送信せず回収できることを通す。commandを記録前に破棄するケースと、記録後にcommandを破棄するケースも分ける。

### 3.3 review可能な分割

| 小単位 | 具体的範囲 | 成功の証拠 |
| --- | --- | --- |
| M1a: backend抽出 | Buffer生成／map／flush／destroyをCPU割当と分離。通常Bufferを同じ処理へ接続。deviceの解決済み参照を渡す入口を用意 | 既存Buffer／mapping／転送テストが通り、native生成回数・必要な待機が増えていない |
| M1b: private layoutとslot | BufferImpl定義のprivate化、compile-only寸法生成、ABI guard、単slot region、construction journal、rollback | 寸法一致・不整合拒否・alignment／capacity／二重構築／未構築resolveのCPU検証が通る |
| M1c: 縦断接続 | owning view、CopyBufferのref＋owner overload、recording／in-flight保持adapter、取消回収 | 実GPUで上記の全経路と旧Buffer混在が通り、破棄イベントとreadbackが一致 |

M1aだけを終えて「Typed arena化完了」とは扱わない。M1cまでが最初の移行単位。逆にM1のために全Phase APIを置換する必要もない。既存Completionへのretain adapterをまず利用し、同期と保持の分離はadapterの内部契約から始める。

M1aの互換adapterには既存のdevice共有所有を一時的に残せる。Impl内部のdeviceを借用へ移し、owner側で外部依存を保持する変更はM1bと合わせて完了する。過渡的な所有と最終契約を区別し、backend抽出だけの最初のPRを無用に広げない。

M1では既存Completionが完了後も資源を保持する挙動をadapterで許容してよい。その場合、`wait()`直後の解放を受入条件にせず、Completion／slotの保持を落とした後の正確な回収を検証する。結果だけ残すCompletionSignalへの変更はM7でreadback ticket等とまとめて移行する。

### 3.4 最初のコード変更候補

パスは仮案。実装時に既存の名前・include構造へ合わせる。

| 現在の箇所 | 最初に変える点 |
| --- | --- |
| `VSLi/VSL/src/vulkan/buffer.cpp`のMakeBuffer／GetData／BufferImpl destructor | CPU確保・native初期化・native破棄・mapping操作を分ける。旧APIはadapterへ残す |
| `VSLi/VSL/include/VSL/vulkan/_pimpls.hpp`のBufferImpl | private定義への切出し、native状態と借用device参照。外部所有はowner側で保持 |
| `VSLi/VSL/include/VSL/vulkan/buffer_and_image_accessor.hpp` | 通常公開APIを保つ橋渡し。新BufferRef／owning viewの入口を追加 |
| 新規`src/backend/buffer_state.hpp`／`buffer_ops.cpp` | placement可能な実体と共通Vulkan操作。arenaやshared_ptrを要求しない |
| 新規`include/VSL/storage/impl_layout.hpp`等 | generated定数の入口、opaque kind、ABI選択。完全定義を公開しない |
| `VSLi/CMakeLists.txt`／関連cmake | probe・assert TU・生成先・install・config条件。private headerをinstallから除外 |
| `commands/copy_buffer.hpp/.cpp` | 借用refと保持ownerを受ける経路を追加。旧BufferAccessor overloadはadapterとして維持 |
| 既存ResourceUse／Completion adapter | 完了・取消とpinの境界を接続。arena内部へ強いsubmission参照を置かない |

**最初に着手する作業はM1a**。既存MakeBufferの振る舞いを固定し、`initialize_buffer_native(BufferImpl&, ...)`と`destroy_buffer_native(BufferImpl&)`を抽出して通常Bufferから使う。単なる関数移動でなく、「この処理はCPU領域とownerを選ばない」という依存条件を検証する。

## 4. 各段階の意味のある検証

### 4.1 M0／M1: 構築・解放・記録

| 検証 | 確認する意味 |
| --- | --- |
| native生成の各段階で失敗注入 | C++構築、createBuffer後、memory選択、allocate後、bind後等。取得したものだけを一度回収し、領域も返る |
| CPU allocation失敗／journal不足 | native生成前に失敗し、半公開handleを返さない |
| alignmentと境界 | misaligned base、over-aligned型、容量ぴったり／1 byte不足、align-upと積・合計のoverflow、空plan |
| slot状態 | 未構築resolve、二重構築、kind違い、破棄済みgeneration、再利用後の旧handleを拒否。解放済みborrowをgeneration検査で安全にできるとは扱わない |
| command作成後に元所有者を破棄 | invoke前のpinを確認する。記録後保持だけのテストで代用しない |
| 記録後にcommandも破棄 | submissionが必要なownerを保持する |
| Completion handleを保存しない送信 | manager側の保持によってGPU完了前の回収が起きない |
| cancel／例外 | 未送信のnative記録を再利用不能にしてlease／pinを解放。記録の一部だけ解放しない |
| 外部旧Bufferとのcopy | device・usage・範囲、部分viewのoffset、host readback値、非coherent可視性が維持される |
| native破棄順 | unmap、buffer破棄、GPU allocation返却、C++破棄、CPU領域返却、外部device／allocator owner解放の順をイベントで確認 |
| 循環検出 | 最後の外部pin解放後にarenaが回収される。weak観測／計数で確認し、単なるプロセス正常終了で代用しない |

失敗注入用backend／memory providerでは、呼出回数だけでなく順序・取得済み資源との対応を記録する。GPUテストはVulkan validationとreadbackを使い、CPU側はASan／UBSanとpoison／guard領域でuse-after-freeや越境を確認する。

### 4.2 M1b: layout生成とartifact整合性

- 実定義から得たsize／alignmentがgenerated headerと一致することをlibrary側static_assertで確認。
- private memberの追加やpacking変更を故意に行い、古いgenerated headerが検出されることを確認。
- 別config／別ABI IDのbinaryとのlinkまたは構築を拒否する。sizeが同じでもrevision不一致を拒否。
- build-treeとinstall-treeのconsumerで同じ契約を確認。prefix移動、binaryとheaderの取り違えも検査。
- multi-configはDebug／Release同時生成と実際のimport mappingを確認。consumer NDEBUGだけで誤選択しない。
- crossではtarget executableを起動しないことをビルドログで確認。targetのsizeがhost値に置き換わらないことも確認。
- Windows MSVC／Linux GCCまたはClang／macOS Clangで実行matrixを設ける。未確認環境を「対応済み」としない。

### 4.3 M2／M3: slot・共有・世代

- 同じImpl型が複数のset／binding／配列要素に現れるschemaで、値とidentityを混同しない。
- SHARED／PER_FRAME混在、疎なset、runtime count、外部資源だけのplan、capacity超過、frame数変更を確認。
- 全slotで構築失敗を注入し、それ以前の依存だけが逆構築順で解放されることを確認。
- 同arena内依存は自己pinを作らず、別arena importはownerを保持し、相互strong依存は登録時に拒否する。
- AHO Bufferの複数aliasが再確保後の同じ新storageを参照する一方、旧view／旧commandは旧storageの内容を参照する。
- 再確保失敗時に論理size・capacity・内容・旧viewが維持される。Customのmapping→Storage→Customizationの順も確認。
- 使用中の更新拒否を移行前より緩めず、非coherent mappingのflush／invalidate範囲を維持する。

### 4.4 M4〜M7: 依存資源・更新・完了

- imageの部分範囲／layout遷移／readback、samplerの外部依存、swapchain imageを誤ってdestroyしないこと。
- descriptorの同binding置換・他binding保持・配列・外部資源を確認。command作成後／記録前の更新が使われ、記録中／送信中の更新は拒否されること。
- TypedBatchのbind省略経路でも使用登録と保持が行われること。
- pool／layout／set／pipeline／shader／render passの依存破棄順を確認し、生成scratchが不必要に長期間残らないこと。
- 複数frame slotの再利用、古いfence結果の保存、異なるsubmissionの同時追跡、manager破棄時のdrainを確認。
- DefaultPhase取消で取得画像を返し、presentation完了をgraphics fenceと同一視しないこと。
- Completionが結果だけを保持する変更時は、完了後に読むreadback ticketやmapping自身が必要なownerを持つことを確認。

## 5. 互換性の導入手順

1. 通常VSLのconstructor・copy／mapping・共有コピーは残し、内部を共通backendへ置き換える。
2. opaque Ref／owning view／placement APIをopt-inで追加する。新旧command overloadをadapterで接続する。
3. schema／bundleの既定経路は旧所有方式のまま検証し、arena policyを明示選択した経路を併設する。
4. `.data_`やshared_ptr<Impl>を直接使う利用側には、viewまたは`ref + owner pin`への移行例を用意する。aliasing adapterの使用箇所を境界へ限定する。
5. 新経路の寿命とABI matrixが揃ってから既定を変更する。private Impl公開や完了後保持への依存はrelease noteに明記する。

公開型名の変更やwrapperの内部サイズ変更がある場合は全consumerの再ビルドを要求する。ソース互換のadapterがあることを、既存binaryとの互換性の証拠にはしない。

## 6. 性能仮説の測り方

責務分離の成立と性能測定を分ける。比較するのは同じschema・資源数・payload・送信・描画結果を持つ旧heap経路とarena経路。

| 指標 | 分けて数える内容 |
| --- | --- |
| CPU確保 | Impl本体、管理表、内部コンテナ、owner control block、submission、payload一時領域 |
| 所有操作 | wrapperコピー、command作成、record、submit、完了解放ごとのOwnerTokenコピー／pin数 |
| 参照経路 | 公開facade→借用ref→backendまでの実アクセス。コンパイラ最適化後の状況も確認 |
| 構築／破棄時間 | 少数／多数の資源、成功と失敗巻き戻し、cold／warmを分ける |
| 記録時間 | 単発command、再利用command、TypedBatch、descriptor切替を分ける |
| 保持メモリ | 長寿命view一つでarena全体が残る量、旧世代再確保時のpeak、内部capacity余白 |
| GPU側 | native生成数、memory allocation数、barrier／submit数、GPU出力・時間。CPU配置の変更だけで減るとは想定しない |

生成時のprobeコスト・consumerのコンパイル時間も測る。結果が出るまでは「一括配置できる」「責務が分かれる」とだけ述べ、速度向上率を示さない。

## 7. 今回の成果と未実施の範囲

今回の成果は設計・移行計画・現コードの参照経路の整理と、寸法生成の小さな調査probe。M1以降の製品実装は行っていない。最初の実装作業はM1aのBuffer backend抽出として開始できる。

Buffer一本のM1が完了するまでは、image・descriptor・pipelineのarena化へ同時に着手しない。名前や抽象クラスを先に大量追加せず、通常BufferとTyped配置の両方が共通backendを使う縦断例で設計を検証する。

## 8. 今回実装したM1と次の境界

M1a〜M1cをホスト可視Bufferの複数slotで実装した。M1bの単slotは要求に合わせてN slotへ拡張し、slotは同型でもkeyとoffsetで独立している。M1cは新しいTyped copy_toから実体を解決済みのCopyBufferへ接続する。旧Typed views／bundle／AHO Buffer再確保の意味は維持する。

設計時には旧Completionの保持が残るadapterを許容していたが、新経路にはterminal callbackを追加し、Completionを保存したままでもpinを解放できるようにした。同期が完了を判断し、lifetime callbackが所有者を解放する。既存全体のCompletionSignal化は未実施。

次はM2のResourceAllocationSchema→CPU plan変換。descriptorを配置する前に、同じarena内部の借用依存と外部owner依存を分け、ResourceUseGroupがowning viewを再帰保持しない接続を検証する。実装と受入結果は[実装報告](buffer-placement.md)を参照。
