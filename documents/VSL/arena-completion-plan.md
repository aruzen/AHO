# VSLの4責務分離とTyped arena化の完了計画

2026-11-08。#325（Image／Samplerの共通backend）を基点とする最新の移行方針。従来の[段階的移行](arena-migration.md)に対し、利用者が次を選択した。

- Instance／DeviceもCPU配置の対象にする。
- 最終的にTypedの既定をarenaへ変更し、heapは明示選択として残す。
- 公開 `.data_`／`shared_ptr<Impl>` 入口も最終的に廃止し、利用側を移行する。
- 内部コンテナとGPUメモリの確保は計測し、別課題として残す。

最初のPRだけで終了せず、下記の順で実装・検証・独立レビューを継続する。PRの公開前には最新masterとの差分を確認する。新しい製品ディレクトリが必要な場合は作成前に確認する。

現在の到達点は順序1〜9と10a〜10dである。command／同期資源の配置とTyped frame再利用、起動資源の共通backend、OS windowの寿命分離、公開StartupArenaとAHO起動、maintenance1の能力照会／有効化、Swapchain世代のCPU配置、取得後失敗とpresent専用の完了観測・資源保持を接続した。順序10全体と11〜12は未完了であり、AHOの世代公開とresize／close、既定policy変更、旧入口廃止と最終監査まで継続する。

10dはPhaseCall、共通Fence用途、M個のfinished semaphore、F個のacquire Fenceに、[送信結果とnative使用完了](native-submission-proof.md)と[AcquireTicket／Surface回収](acquire-ticket-recovery.md)を接続した。実enqueueに従うsignal drain／画像返却、OOM retry、Native中の取消、Domainを失った後の回収を検証した。次は10eであり、11、12も引き続き未完了である。

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
| 9 | Instance／PhysicalDevice／Surface／Device | 起動資源を寿命群別arenaへ配置し、子Implの強い所有とshared_from_this依存を除去 |
| 10 | swapchain／presentation／AHO | 取得画像・世代・presentation完了まで保持し、Engineの起動・resize・終了を新経路で通す |
| 11 | Typedのarena既定化 | 標準policy変更、heapの明示指定、AHO／サンプル／テストの既定経路を移行 |
| 12 | 旧入口の廃止と監査 | 公開data_／shared_ptr<Impl>と移行adapterを除去し、残る確保・参照・保持量を測定 |

完了管理は主要資源を配置した後、descriptor等の展開より前に分離する。後続資源が旧Completion所有構造へ依存し続けないようにする。

## 起動と所有境界

起動時の生成順と破棄単位を分ける。既定は共通Instance rootを先に完成し、Device群（選択したPhysicalDevice／Device）と、WindowごとのSurface群を別ownerに置き、それぞれ元Instanceをimportする。Device生成中だけSurfaceを保持し、Deviceの固定親にしない。SwapchainはDevice群とSurface群の両方を保持する。Deviceを必要とするBufferが残っているだけで、無関係なSurface／OS windowを延命しない構成を基本にする。

Instance／Device／Surface等を単一ownerに同居させる配置は、寿命を揃えたい場合の明示選択に限る。依存辺を外しても同一owner内の破棄単位は分かれないため、複数Window／Window再作成の既定にしない。候補列挙はscratchで行い、選択後の能力情報で後続のarenaを計画する。PhysicalDeviceのnative handleはInstanceに属する借用として扱う。

Deviceの正規ownerは起動側に置く。内部はdevice refを借用し、公開getter等で所有が必要な場合だけ借用contextと弱いowner参照から取得する。getterで子image／bufferを保持するaliasを返さない。通常のVulkan操作ではこの所有解決を行わない。

OS windowは外部依存とする。Surfaceが保持するOS window ownerと、Runtimeを参照するAHO Window facadeを分離する。起動owner→OS owner→起動ownerという循環を作らない。graphics fenceの完了とpresentation完了は区別する。

## 公開APIの切替

移行中は旧入口をadapterで併設し、削除する段階を明記する。最終的には通常VSLもowning handle／viewと借用refを使用する。現在のbuffer.data_等は、操作ではbuffer.ref()、保持が必要な境界ではbuffer.view()を渡す形式へ移行する（名称は仮案）。heapとarenaで生成・検証処理は共用する。

Typedの既定変更は、主要資源と起動・終了の経路が揃ってから行う。旧公開入口の廃止時に利用例・破壊的変更の一覧を用意する。

arena既定化より先に、[Typedの配置・保持期間・固定依存](typed-lifetime-boundaries.md)を分離する。heap／arenaの記録保持はCOMPLETIONを共通既定とし、旧SUBMISSIONは明示選択とする。管理recordは資源種ごとの参照欄を増やさず、固定schemaの依存を公開前にfreezeする。通常操作での管理表再探索や所有graph DFSは追加しない。

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

6cは[schema付き共有Layout集合](typed-shared-layouts.md)をbundleからTyped pipelineへ接続した。Resources注入が既存Layoutをheap再生成する経路を置き換え、set番号のholeは空Layout一つで埋める。必要時の追加slot／IDをshared計画の末尾へ置き、宣言emptyを優先して再利用する。frame facadeを保持せず、元bundle／source回収後にも利用できる。通常／共有constructorのshader検証、既存配置、Set／Pool容量、実compute／描画／resize／installを確認し、順序6の受入条件を満たした。次は順序7へ進む。

## 順序7／8の前提調査と次の実装

6c時点の独立レビューにより、次を順に扱う。

1. RenderPassの共通operation／lifetime backendと配置。不変descriptionは記録・Framebuffer検証にも必要な値として保持し、不要なrecipe所有層を追加しない。Deviceの強い所有はrootへ移す。descriptionの借用はlvalue所有者に限定する。
2. Pipelineの共通backendと配置。native Layoutを生成しないstate定義を用意し、arenaのGraphics入力はLayout ID・Pass ID・subpass・state・Shader IDまたは構築時sourceへ分ける。既存Frozen constructorも同backendへ接続する。定義取得のための余分なheap Layout生成を恒久化しない。
3. Framebufferの共通backendと配置。まず一つのImplに複数native framebufferとattachment行列を持つ既存の意味を維持し、Pass／Image／swapchain所有を外側rootへ移す。同rootの参照は借用とし、getterは本来の正規ownerを返す。

7aは[RenderPassの共通backendとCPU配置](render-pass-arena.md)を実装した。不変descriptionをImpl内へ置き、通常／arenaの生成、外部importの正規owner、失敗時rollback、lvalueの借用制約とGPU完了／取消までの保持を検証した。Pipeline／Framebufferの配置は未実装であり、順序7全体は未完了である。次は7bへ進む。

7bのstate入力は、Device／native／source ownerを含まない`GraphicsPipelineState`の深い値型にする。公開値は不完全な内部データをunique_ptrで所有し、既存Recipeは同じデータ型をinlineで持つ。これにより既存Layoutにはstate用の追加確保を増やさない。固定stageの注入本体を借用EditRefへ移し、旧Layout注入は委譲する。生成入力のCPU確保はnative資源配置と区別して監査する。Shader／Pass／Layoutを必要とするstageをstate-only入力へ混ぜない。Typedの入口では実Shaderを検証し、型名だけを付ける無検査の変換を追加しない。

[7b前半のstate分離](graphics-pipeline-state.md)を独立した変更として実装・検証した。

7b後半のImplはDevice／Layoutへの借用とnative handleだけを持ち、heap rootはLayout ownerを保持して依存graphへ列挙する。native生成はResultを明示検査し、失敗時の非null handleを未公開のまま回収する。明示入力のShader／Passはnative生成呼出し中だけ所有を借り、Layoutはruntime ownerだけを固定親とする。Shaderの既存get_device()だけでは非所有aliasを拒否できないため、source境界でowner／live module／Deviceを検査する。構築とrollback、importの正規control block、逆向きdescriptor依存拒否を受入項目にする。

[7b後半のPipeline backendと配置](pipeline-arena.md)を通常VSL／Typedの既存生成とResourceArenaへ接続した。直接source回収後の描画／compute／push、Layout固定親の循環拒否、元ownerのimport、途中失敗・同region再利用、GPU完了／取消回収を検証し、7bの受入条件を満たした。

7cのPass getter用weak metadataはlifetime contextへ分け、Implと同じslotの隣接byte領域へ構築する。Implはcontextを借用し、通常のVulkan操作はraw Pass／Imageへ直接届く。slot本体はstandard-layoutなaligned byte配列だけとし、Impl先頭offset0とactual slot寸法を検査する。CPU配置計画にはcontext込みのslot寸法を公開する。direct Pass／Imageは固定親として永続保持し、同rootのPass weakはpublish後に設定する。getterは外部Passの元control blockを返す。

Pipelineが記録時に使うLayoutは保持し、Shader／Pass／再生成recipeは生成時依存へ分ける。Framebufferから既定Passとclear descriptionを参照するVSLの契約は、native生成時だけの依存と区別する。attachment Imageの既存物理UseStateを使い、RenderPass／Pipeline／Framebufferへ独立したGPU UseStateは追加しない。

[7cのFramebuffer backendと配置](frame-buffer-arena.md)で、native生成前の全行検証と配列確保、途中失敗の逆順rollback、元のPass ownerを返すgetter、共通Image状態での記録を実装した。保存command／旧view、CPU世代の失敗保証、互換Pipeline再利用、記録中poll callbackの取消・自己破棄・再入を検証し、順序7の受入条件を満たした。WSIのretired swapchainからの復旧とpresentation完了は順序10に残す。

順序8の配置前に、CommandBufferImplのsubmissions／recording_submission／presentation_resourcesを外部domainへ移す。現在のImpl→Work→Device/resourcesのまま同arenaへ入れるとroot→Work→rootの循環になる。外部Registry→Work→起動／資源root、Impl→contextの借用／weak、Completion→結果だけという向きを確定する。通常invokeは解決済みcontextへ直接届き、Registryの探索はphase境界に限る。shutdown時の新規登録拒否・snapshot終端処理と、DefaultPhase cancelの取得画像返却用の内部送信も検証する。presentation完了の証明は順序10へ残す。

## 順序8の分割

8aは[Command Domain](command-domain.md)でWork registryを外へ移す。nativeのheap生成はこの単位で維持し、元facade破棄、同native ownerのWork保持、完了callbackからの再送信、準備中のpeer保護、wait_idle中のPENDING維持、取消時の旧SUBMISSION保持を検証する。

8b前半はCommandPool／CommandBufferの共通operation・placement構築・寸法生成とResourceArenaへの配置。Buffer→Pool→Deviceの固定親を正規化し、明示的な配置入口でだけDomainをattachする。Domainをarena rootへ強所有させない。attach用配列確保はpublish前に終え、終了callback中の再attachは拒否する。reset／nextは記録・取消・対象slotのGPU使用を検査し、全resetは事前に全slotを検査する。

[8b前半の実装](command-arena.md)で、通常VSL／arenaの共通backend、同rootの複数ring、Domain再接続、元Poolのgetterと固定辺、Typed copyと自己root保持、逆rollback、resetの全slot事前検査・再入・世代照合を検証した。nextはnativeを変更せずhost巡回だけなので、送信済みslotへも進める。GPU待機は次Phaseのprepareが行う。

8b後半はFence／Semaphoreの共通backendと配置、明示destroy／resetの使用制約、名前付き登録と古いHolderの同名置換を扱う。Fenceは未送信予約から保護し、SemaphoreのWSI待ちはgraphics結果と分離する。native配列のCPU容量を生成前に確保し、部分失敗を逆順で巻き戻す。

[同期資源の共通使用状態](synchronization-state.md)で、Fence要素別のPENDING予約／公開wait拒否／全対象先行検査、Semaphoreの複数graphics結果とWSI marker、cleanupへの予約・元sourceの移譲、named replacement保護を実装した。native配列生成・破棄・待機／resetも共通operationへ移した。次はそのbackendを指定領域から使うFence／SemaphoreのCPU配置である。

8cでTyped frameの配置・再利用へ接続する。8aの所有分離だけでは順序8全体の完了と扱わない。

[8b後半の同期資源配置](synchronization-arena.md)で、通常の名前付き生成とarenaのbackendを共通化した。Implはraw Deviceと保存native Deviceを使い、隣接contextはweak registryだけを保持する。同Rootの異なるSemaphore／FenceをWorkの1所有要素で保持しつつ、各実体の使用状態を維持する。nativeの明示破棄、CPU slot寿命、importの正規owner、途中失敗・同region再利用、Typed転送と完了／取消回収を検証する。

8cは実行用資源とper-frameのデータ資源を別ownerに分ける。独立ComputeではPool×1、ring Impl×1（native CB×F）、Fence Impl×1（native×F）を実行群へ配置し、同じManagerを既存FrameResources／Bundleへ渡す。外部FenceをPhaseへ明示して内部heap Fence生成を回避する。Manager／DomainをFrame値やarena rootへ強所有させず、Domain→Work→Frame→Domainの循環を作らない。Computeの巡回は明示、DefaultPhaseの巡回は既存のsetupへ残す。

AHOではEngineのcommand群、Windowのframe同期群（image-available×F／in-flight Fence×F）、swapchain世代のrender-finished×Mを分ける。Fと画像数Mを混同しない。DefaultPhaseが別途生成するacquire Fenceはgraphics Fenceと兼用せず、WSI予約・取消返却・厳密完了とともに順序10で扱う。

[8cのTyped frame実行arena](typed-frame-command-arena.md)で、明示heap／arenaの共通facade、元Managerとringの固定、外部FenceによるPhase生成、明示巡回を接続した。3slot反復・取消・Frame値の完了解放・Facade破棄後の使用・組全体の移動代入を検証し、両policyのuniform／descriptor実computeとarena版bundleの配列／readbackを確認した。これで順序8の受入条件を満たす。AHOの生成移行とWSI acquire Fenceは順序9／10へ残す。

## 順序9の着手単位

9aはInstance単体の共通operation／lifetime backendと指定領域構築。validation有無の実体をruntimeデータへ揃え、旧Vulkan facadeはadapterとする。所有値の生成入力とGLFW拡張収集を分け、Instance→debug messengerの途中失敗を巻き戻す。プロセス全体のstdio／console変更をlibrary backendへ持ち込まない。

[9aのInstance backendと指定領域構築](instance-arena.md)を実装した。通常heap／配置の共通化、runtime型の統一、所有入力、正規owner、失敗出力未採用・逆回収・同slot retry、releaseのhost descriptor維持、構成別／cross／install／artifactを検証した。ResourceArenaへDevice以前の資源を混ぜず、Instanceのlayout traitを公開した。公開StartupArenaとPhysicalDevice以降は未実装であり、順序9全体の完了とはしない。

[9bのPhysicalDevice backendと指定領域構築](physical-device-arena.md)で、候補scratch／選択済み実体と旧custom searcherのowning候補を分けた。heap／配置の共通構築、Instance固定親と元ownerのgetter、同型slotの直接参照、能力cache、失敗rollbackを接続した。Device前提のResourceArenaへstartup kindを混ぜず、PhysicalDeviceのlayout traitを生成・配布する。

9cはSurfaceのOS owner／GLFW leaseと共通backend／指定領域構築である。WindowDataの論理的closeとnative windowの最終破棄を分ける。OS ownerはplugins／Engineを持たず、Surfaceの外側ownerがInstanceとOS ownerを固定保持する。GLFW callback内ではnative破棄を延期し、管理側のmain-thread境界で回収する。leaseの共有範囲と外部GLFW利用の契約を明記し、Surface／Swapchainだけが残る場合にもGLFWを終了しない。

[9cのOS window／Surface実装](surface-arena.md)で、独立OS leafとGLFW lease、main thread退役queue、Surfaceの共通backend／指定領域／元親getterを接続した。GPU／CPU／compile／配布構成と同一画像の回帰を検証し、9cの受入条件を満たした。次は9dへ進む。

9dはLogicalDeviceの固定親と正規owner、共通backendと指定領域構築を対象にする。子のDevice getter等に残るshared_from_thisを置換する。Deviceを依存graphへ公開する時は、既存heap leaf／ResourceArena／visitorにも正規Deviceの固定辺を列挙する。

[9dのDevice backendと正規owner](device-arena.md)で、通常heap／配置の共通生成、所有するqueue／feature入力、元PhysicalDeviceの固定親、子の元Device getter、全Device辺の公開、未公開Deviceからのraw子構築を実装・検証した。Device自身の正規ownerと既存のopaqueな子ownerは区別する。生成境界でownerを検査し、通常操作ではraw実体参照を利用する。

当初9dへまとめていた公開起動arenaとAHO起動は9eへ分割する。Deviceの共通backendと、その利用者である配置ownerの変更を独立に検証するためであり、順序9の完了条件は減らさない。

9eではDevice引数不要の公開StartupArenaを作る。共通Instance root、選択PhysicalDevice／Deviceの群、各WindowのSurface群を別ownerとして構築し、既存の生成処理と寸法情報を使う。同型slot、外部Instance、失敗rollback、元ownerのgetter、Deviceだけを残したWindow回収を検証し、AHOの起動／終了と複数Windowへ接続する。候補列挙scratchと実体の永続配置は分け、公開前の同群依存はraw参照にする。

[9eの公開StartupArenaとAHO起動](startup-arena.md)で、生成済み寸法からのconstexpr計画、入力snapshot、親順構築と成功逆順rollback、元owner getter、AHOの三寿命群を接続した。Device生成中だけSurfaceを保持し、Deviceだけが残る場合のSurface／OS独立回収、複数Window、同領域retry、Typed転送を検証した。これで順序9の受入条件を満たす。

順序10ではSwapchainの共通backend／配置、graphicsとpresentationの完了の区別、取消・退役世代の保持と再生成失敗からの復旧、acquire Fenceを扱う。9cで確認した無効resize引数のnative前拒否と、最小化で0になったframebufferの再生成延期も必須の検証項目とする。

## 順序10の分割

10aはmaintenance1の能力照会・Instance依存・Device feature chainを接続する。厳密な資源回収を要求する入口では未対応を生成前に拒否し、互換fallbackは独立policyとする。現SDKのEXT型と実driverの能力を照合し、device idleをpresentation完了の証明へ昇格させない。

[10aのPresent Fence能力と有効化](present-fence-features.md)で、独立した生成要求、実Instance設定、core／KHR queryとtimeline併用、supportedとenabledの分離を実装・検証した。拡張名だけやLEGACYでfeatureを暗黙に有効化しない。実際のFence付きpresent／厳密回収は10cで接続し、この段階では保証しない。

10bはSwapchainの共通backend／指定領域構築と、全aliasで共有する現役／退役状態。nativeへoldSwapchainを渡した後は、生成失敗でも退役を取り消さない。実画像数に従うImage／世代同期群の計画を、native pending状態から構築する。

[10bのSwapchain世代とCPU配置](swapchain-arena.md)で、native Pending→実画像M→CPU領域→Image slots→世代公開を接続した。全aliasの退役状態、元親owner、途中失敗の逆順rollbackと同領域再利用、準備callbackの世代変更拒否、実取得世代／indexによる取消を検証した。取消の使用登録は同じ物理ImageUseStateへ非pollで接続し、取得情報はGPU完了まで保持する。世代同期群の実present証明とacquire Fenceの再利用は10c／10dへ続く。

10cは[Presentの完了観測と世代保持](presentation-retention.md)で、Surface共通の外部Domain／PresentRecord、世代内のFence pool、enqueue結果の分類、snapshot回収とcallback再入を接続した。graphics完了とstrict present証明を分け、DeviceLostは全snapshotへ先に保存してから保持を解除する。不明結果の終了は退避し、同Surface／Deviceに限定して非blocking回収する。Swapchain内部からRecordを強所有しない。

10dはframeごとのacquire FenceとAcquireTicket、取得後失敗・取消時のsignal消費／画像返却、present OOMのretryと破棄を扱う。[前半の準備中の借用寿命](phase-call-lifetime.md)で、stack guardによる自己破棄検知、局所ownerの解放・例外後の中断、共有Fence予約と既存peer-start契約を検証し、後続のTicketと共通回収へ接続した。

[Fenceの共通用途状態](fence-use-kinds.md)で、graphics／acquire／presentの物理要素を一つのweak variantへ整理した。strict poolの正規aliasも同じ予約・変更検査を使い、旧Proof保存後のreset、UNKNOWNのpoll限定、配列Lossの全結果保存後通知を検証した。Ticketへ別の物理使用表を追加しないための前提であり、F個の実acquire poolと取得後回復へ続く。

[世代の描画完了Semaphore](generation-semaphores.md)で、既定経路のUniqueSemaphore vectorを共通backendと世代内のSemaphoreImpl slot（native実画像M個）へ接続する。AHOは同Mのresizeでも新sourceへ更新する。同期source３役はCOMPLETION保持へ統一し、送信履歴で旧世代を延命しない。

[取得Fence pool](acquire-fence-pool.md)でF個のnative Fenceを共通ResourceArenaへ配置し、Domainに正規source一つだけをlazy保持する。CPU-only proofの保存・予約identity・外部source・取得失敗・reset巻戻しとcallback中の自己破棄を検証した。

[取得画像の責任とSurface回収](acquire-ticket-recovery.md)で、実Ticket／graphics／drain／presentの結果を分離し、同じ物理Semaphore要素のlease、独立した取得Fence証明、画像release、RECOVERY_REQUIREDを接続した。全CPU状態保存→全bookkeeping→owner解放の共通batchを、公開Surface APIとCommandDomain待機へ適用する。自動present回収は従来の描画Completion通知順を維持する。新16件と全VSL1,112件、AHO161／1,070、compile195、install27、構成別／cross／ABI、同一描画の検証が成功した。

10eはAHOのframe同期群とSwapchain世代群を分け、候補生成後のattachment失敗から復旧する。0 extentでは再生成・acquire・Fence resetを延期し、復元後に一括公開する。最後のpresent直後close、連続resize、marker保持量の有界化と互換Pipeline再利用を検証して順序10完了とする。

[10e1のWindow frame同期群](../AHO/window-synchronization.md)で、available／graphics Fence／acquire FenceをWindowの同rootへ配置し、M finishedと分離した。Engineの明示policyを初回・追加Windowへ伝達し、Window個別override、旧custom source省略、Ctorの一回snapshotを接続した。AHO GPU168／CPU1,071、compile195、public consumerと同一描画が成功。candidate保持、0 framebuffer、close統合を次の単位で続ける。

