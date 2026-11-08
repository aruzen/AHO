# VSLの4責務分離とTyped arena化の完了計画

2026-11-08。#325（Image／Samplerの共通backend）を基点とする最新の移行方針。従来の[段階的移行](arena-migration.md)に対し、利用者が次を選択した。

- Instance／DeviceもCPU配置の対象にする。
- 最終的にTypedの既定をarenaへ変更し、heapは明示選択として残す。
- 公開 `.data_`／`shared_ptr<Impl>` 入口も最終的に廃止し、利用側を移行する。
- 内部コンテナとGPUメモリの確保は計測し、別課題として残す。

最初のPRだけで終了せず、下記の順で実装・検証・独立レビューを継続する。PRの公開前には最新masterとの差分を確認する。新しい製品ディレクトリが必要な場合は作成前に確認する。

現在の到達点は順序1〜5と6aのShader／6bのPipelineLayoutである。順序6c以降は未完了であり、起動資源、既定policy変更、旧入口廃止と最終監査まで継続する。

## 責務と保証

storageはCPU領域と配置、operationは解決済み実体へのVulkan操作、lifetimeは構築・所有・回収、syncは使用状態と完了観測を担当する。通常VSLとTypedでbackendを共用する。操作中にownerを何段も解決せず、公開view／command／外部依存の境界で所有を取得する。

CPU arenaにはImplと使用状態の本体を含める。内部vector／map、生成scratch、Vulkan device memoryは別の確保として明示する。同型の複数資源はResourceIdとslotで区別し、BindingKeyはResourceIdへの接続情報とする。shared／frame／transient／起動／swapchain世代等の寿命群を別ownerに分け、生存中のarenaは移動・拡張しない。

同じarena内は借用だけ、別arenaの依存はownerを保持する。依存は非循環とし、構築前に検査する。外部入力は元のImpl・物理UseState・ownerを使い、新しい使用状態へ包み直さない。構築失敗は成功済みの実体だけを依存の逆順で回収する。

## 実装単位と完了条件

| 順序 | PR単位 | 完了条件 |
| --- | --- | --- |
| 1 | 異種Implの寸法と配置計画 | Buffer／Image／Samplerの実寸法生成、private定義との一致、ResourceIdとslotの分離、runtime個数、外部入力、公開ヘッダーだけのconstexpr配置計算 |
| 2 | 異種ResourceArena | 共通backendを指定領域から使用し、直接参照・所有・失敗時rollback・完了後破棄を通す |
| 3 | Typed image／sampler | 寿命群ごとのplanとbundleでbuffer／image／samplerを混在させ、upload・sampling・readback・完了解放を通す |
| 4 | 完了結果と資源保持 | 結果を保存してもin-flight資源を保持し続けない。取消・送信失敗・device lostの終端処理とreadback所有を整理 |
| 5 | descriptor layout／pool／set | 共通backendと配置へ接続し、runtime容量・記録時binding・更新制限・bind省略を維持 |
| 6 | shader／pipeline layout | 生成scratchと永続実体を分離し、共有layout・外部shaderを利用可能にする |
| 7 | render pass／pipeline／framebuffer | 依存資源を借用して配置し、resizeのattachment世代更新と互換pipelineの再利用を維持 |
| 8 | command／同期資源／frame | command pool・command buffer・fence・semaphoreを分離／配置し、記録→送信→frame再利用を接続 |
| 9 | Instance／PhysicalDevice／Surface／Device | 起動arenaへ配置し、子Implの強い所有とshared_from_this依存を除去 |
| 10 | swapchain／presentation／AHO | 取得画像・世代・presentation完了まで保持し、Engineの起動・resize・終了を新経路で通す |
| 11 | Typedのarena既定化 | 標準policy変更、heapの明示指定、AHO／サンプル／テストの既定経路を移行 |
| 12 | 旧入口の廃止と監査 | 公開data_／shared_ptr<Impl>と移行adapterを除去し、残る確保・参照・保持量を測定 |

完了管理は主要資源を配置した後、descriptor等の展開より前に分離する。後続資源が旧Completion所有構造へ依存し続けないようにする。

## 起動と所有境界

起動用arenaにはInstance・選択したPhysicalDevice・必要なSurface・Deviceを配置する。候補列挙はscratchで行い、選択後の能力情報で後続のarenaを計画する。PhysicalDeviceのnative handleはInstanceに属する借用として扱う。

Deviceの正規ownerは起動側に置く。内部はdevice refを借用し、公開getter等で所有が必要な場合だけ借用contextと弱いowner参照から取得する。getterで子image／bufferを保持するaliasを返さない。通常のVulkan操作ではこの所有解決を行わない。

OS windowは外部依存とする。Surfaceが保持するOS window ownerと、Runtimeを参照するAHO Window facadeを分離する。起動owner→OS owner→起動ownerという循環を作らない。graphics fenceの完了とpresentation完了は区別する。

## 公開APIの切替

移行中は旧入口をadapterで併設し、削除する段階を明記する。最終的には通常VSLもowning handle／viewと借用refを使用する。現在のbuffer.data_等は、操作ではbuffer.ref()、保持が必要な境界ではbuffer.view()を渡す形式へ移行する（名称は仮案）。heapとarenaで生成・検証処理は共用する。

Typedの既定変更は、主要資源と起動・終了の経路が揃ってから行う。旧公開入口の廃止時に利用例・破壊的変更の一覧を用意する。

## 検証と進捗の扱い

各単位で配置・alignment・overflow・未構築参照・二重構築・外部所有・途中失敗・逆順破棄を確認する。元wrapper破棄後の利用、入口をまたぐ共通状態、循環不在、取消／送信失敗／完了後の回収を検証する。

GPUでは転送・sampling・compute・描画・readback、descriptor更新、旧世代、resize、pipeline再利用、取得画像の取消とpresentation終了を確認する。install consumer、異なるartifactの拒否、構成別生成、クロスコンパイル、compile契約、規約検査を併用する。実行できない環境を明記する。

最終比較は同じ処理・結果のheap／arena経路で行い、CPU確保数・owner操作数・保持メモリ・時間を計測する。性能改善は計測前に断定しない。

各PRの実装・検証結果を関連文書へ追記し、この一覧の終了条件を満たした単位だけを完了とする。Instance／Device、既定変更、旧入口廃止まで終えるまでは、全体完了と扱わない。

順序1の実装と受入検証は[異種資源のCPU配置計画](resource-placement-plan.md)を参照。次は順序2のResourceArenaである。

順序2は[ResourceArena](resource-arena.md)へ接続する。通常operation、外部owner／物理状態の共有、逆順rollback、GPU完了／取消回収を検証した後、順序3のschema／bundle拡張へ進む。

順序3は[Typedのowned Image／Sampler](typed-owned-resources.md)へ接続する。component単位のID共有、寿命群の分離、heap互換、upload／sampling／readbackを扱う。

順序4は[結果とin-flight所有の分離](completion-ownership.md)で、保存結果による資源延命、terminal、取消／失敗、readbackを整理した。次は順序5のdescriptorである。WSIの厳密な完了証明と失敗からの復旧は順序10に残す。

## 順序5の分割

descriptorは配置を始める前に、既存の使用状態と更新metadataを揃える。

1. Setの物理UseStateをnative／Typedで共有し、bindingを配列要素単位で管理する。native scalar updateが要素0だけを書いたとき、未更新要素のownerを解放しない。logical constraintと初期化状態も同じ表へ集約する。
2. Layout／Pool／Setの共通backendと配置へ接続する。LayoutにはGPU UseStateを持たせず、Poolはhost側の排他と各Setのidleを管理する。依存順構築・逆順rollbackと正確な容量を検証する。
3. Typed schemaへ接続する。同rootの物理／logical aliasは内部borrowへ正規化し、可変の外部依存をcommit前に検査する。既知の管理root間の循環と、opaque owner内に隠れた循環に対する保証を区別する。

Layout数は宣言set数のResources::COUNT、Set数はshared／frameの実数、Poolはdescriptorを持つ寿命群ごととする。最大set番号＋1のSET_COUNTをSet実体数へ混ぜない。pipelineのhole用empty layoutは順序6で扱う。

5aの実装契約は[Descriptorの共通使用状態](descriptor-state.md)を参照。

5bは[共通backendとheap所有の移行](descriptor-backend.md)と[寸法生成・ResourceArenaへの配置](descriptor-arena.md)を実装した。同rootの物理／論理ownerを借用へ正規化し、既定FIXEDでは構築時に固定した依存以外のowner追加を拒否する。

5cは[constexpr descriptor論理計画](typed-descriptor-plan.md)を先に分離する。Layout宣言、shared／frame別のSet実数とPool容量を固定配列で計算し、生成用scratchと資源ID／slot割当は後続接続へ分ける。descriptor実体のbundle内配置、可変依存graphと追加logical constraintの接続までは5c全体を完了としない。

[所有参照とroot依存graph](owner-dependencies.md)で、既知ownerの出所、固定親、更新のcycle検査とcommitを接続した。AHOのBuffer generation R→physical storage PとTexture Data D→Image Pも同じgraphへ接続し、弱参照の出所、世代切替のcommitと失敗rollbackを維持する。

[Typed bundleのdescriptor配置](typed-descriptor-placement.md)で、schemaからshared／frameのLayout・Pool・SetをResourceArenaへ展開した。data-only配置との互換、正確な群別容量、固定親、DYNAMICな外部入力、記録時binding、更新制約、完了／取消回収を実装・検証した。これで順序5a／5b／5cの受入条件を満たす。Manager引数等の互換入口は順序12の廃止対象として残し、次は順序6のshader／pipeline layoutへ進む。

## 順序6の着手単位

6aは[Shaderのoperation／lifetime backend、heap所有、寸法生成、ResourceArena配置](shader-arena.md)を実装した。native実体へGPU UseStateを持たせず、stage create-infoとpNameを生成scratchへ分け、entry／codeの永続値と外部Shaderの正規ownerを維持する。配置・失敗・import固定親・Frozen再生成・実描画／compute・install／artifactを検証した。

6bは[PipelineLayoutのruntime／recipe／scratchとCPU配置](pipeline-layout-arena.md)を実装した。runtimeとrecipeのcontrol blockを分け、Pipeline／保存PushConstantはruntimeだけを保持する。Typed pushはruntime内のcanonical rangesから記録する。recipeのShader／DescriptorLayoutは借用と外部source所有へ正規化し、ownerを重複排除する。DescriptorLayout→PipelineLayout、Pool／Layout→Setの構築順と実構築の逆順rollbackを保証する。明示ID／importはarena全寿命、直接のBindingLayout sourceは構築呼出し中だけ保持する。FromRuntimeは構造を照合し、freezeはnativeを再生成せず編集aliasのrecipeだけを分離する。

6cはschema付き共有Layout集合をTyped pipelineへ渡す入口を作る。現在のResources注入が既存Layoutをheap再生成する経路を置き換え、set番号のholeは生成時に扱う。frame facadeをpipeline rootへ保持しない。Vulkan native依存と再生成用recipeの所有依存を区別し、それぞれgraphと寿命を検証する。
