# Pipelineの共通backendとCPU配置

[完了計画](arena-completion-plan.md)7bの後半。[state分離](graphics-pipeline-state.md)で用意した入力を、通常VSL／Typedの既存生成とResourceArenaから同じbackendへ渡す。

## 四責務と依存

operationは解決済みLayout／Pass／Shader参照とfixed stateを受け、共通の検証・native生成・破棄を行う。stage create-info、配列pointer、cacheは生成呼出し中のscratchである。単数pointer APIのResultを明示検査し、失敗時に返された非null handleを公開前に回収する。失敗をログだけで処理して空Pipelineを公開しない。

lifetimeは指定slotへのImpl構築と失敗時回収を行い、heap ownerも同じ処理を使う。PipelineImplはowner node、Device／Layoutへの借用、保存済みnative Device、Pipelineとbind pointだけを持つ。Impl自身はDevice／Layout／arenaを強所有しない。storageは実寸法・alignmentとCPU領域を担当する。

Pipelineは生成後不変なので個別GPU UseStateを追加しない。syncのCompletionは完了結果を観測し、lifetimeのWorkがPipeline ownerをGPU使用完了まで保持する。SUBMISSION保持とWork slot再利用／取消の既存契約を維持する。

Layoutは記録時のdescriptor bind／pushにも必要な永続依存である。heap ownerはcanonical Layout ownerを保持し、DependencyNodeのvisitorにもこの辺を列挙する。Layoutを含むarena AのSetへheap Pipeline Hをlogical ownerとして追加するA→H→Aを拒否する。辺のないleaf ownerとして扱わない。

## 通常VSLと配置入力

既存の`GraphicsPipeline(layout, pass, subpass)`／`ComputePipeline(layout)`とFrozenは維持する。recipeとnative Layoutの整合性を検査し、生成中だけrecipeを借用する。生成済Pipelineがrecipe／Shader／SPIR-V／fixed stateを保持することはない。

recipeを持たないruntime Layoutには生成入力を明示できる。

```cpp
const std::array<vsl::ShaderAccessor, 2> shaders{vertex, fragment};
vsl::GraphicsPipeline graphics(layout.runtime(), pass, state, shaders);
vsl::ComputePipeline compute(layout.runtime(), compute_shader);
```

RuntimeLayoutSourceはResourceIdまたはLayout Accessor、Pass／Shader sourceはResourceIdまたは直接Accessorである。Layoutの直接入力はruntime ownerだけを固定親として保持し、recipeをrootへ持ち込まない。直接のPass／Shaderは初期化呼出し中だけ保持する。明示ID／importはarenaに宣言した寿命を維持する。

```cpp
namespace t = vsl::typed;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 2> requests{{
    {{10}, vsl::storage::ResourceKind::PIPELINE},
    {{20}, vsl::storage::ResourceKind::PIPELINE_LAYOUT}}};
constexpr t::ResourcePlan<2> plan(requests);
const std::array<l::ResourceInit, 2> inputs{
    l::GraphicsPipelineInit{t::ResourceId{20}, pass, state, {vertex, fragment}},
    l::PipelineLayoutInit{}};
l::ResourceArena arena(device, plan, inputs);
vsl::GraphicsPipeline command(arena.pipeline({10}));
```

Pipelineのslotが先でも、独立資源→Layout／Set→Pipelineの順で構築する。成功した順を記録し、失敗時にはその逆順で破棄する。同arenaのLayoutはraw借用、別ownerはcontrol blockとprovenanceを保って重複排除する。

外部Pipeline importのgetterは元の正規ownerを返す。importer全体を包み直すownerを作らない。`GraphicsPipeline(PipelineAccessor)`／`ComputePipeline(PipelineAccessor)`はlive ownerとbind pointを検査するnative commandへの接続であり、Shader schemaを検査しないTyped変換ではない。既存Typed生成は実Shader／vertex／resource／pushの検査と末尾Input注入を維持して共通backendを使う。

ShaderAccessorの既存get_device()だけでは非所有aliasを拒否できないため、直接sourceの境界でowner／live module／Deviceを検査する。生成中のsourceとstateの変更は呼出側で直列化する。通常の記録は保持済みImpl／native handleを利用し、owner graphの探索やslot再解決を行わない。

## 寸法、残る確保と互換性

ResourceKind::PIPELINEとImplLayout／ResourceSlotLayoutをcompile-only probe、private実定義とのstatic_assert、構成別生成header、artifact fingerprint、installへ接続する。USE_OFFSETはNO_SEPARATE_USEとする。GPU device memoryやdriver内部のPipeline確保はCPU slotのサイズに含めない。

定義値、内部vector、native生成scratch／cache、heapの外側ownerは個別確保として残る。arenaの管理metadataと外部依存の保持も残る。配置APIは存続し、公開data_／shared_ptr<Impl>入口は12fで閉じた。Graphics／ComputeはAccessorを値で持ち、値変換で既存commandへ接続する。通常Graphics生成はsnapshot後のNative生成中だけShader一覧とstateを独立保持し、完成Pipelineへ残さない。ResourceArenaの初期化値は引き続き生成中に変更しない。[現在の移行契約](arena-migration.md)を参照。性能率を改善したとは断定しない。

private PipelineImplのdevice／layoutはshared_ptrから借用pointerへ変わる。private定義を読む利用側は`.get()`等を外し、保持が必要なら公開owning facadeを保存する。公開の既存生成引数は維持する。記録用に空／非所有／別Deviceの入力を渡すことと、誤ったbind pointへの接続は拒否する。

後続の[Framebufferの共通backendと配置](frame-buffer-arena.md)で、複数native framebufferとattachment行列、Pass／Imageの固定親、CPU世代の失敗保証と互換Pipeline再利用を検証した。次はcommand／同期資源を分離する。

## 検証

CPU2件・GPU8件・public compile1件を追加した。同型slotと生成寸法、逆順に宣言した依存の構築、native Pipelineを一つ生成した後の失敗と同領域再利用、直接sourceを破棄した後の描画／dispatch／push、正規Device getter、Heap Pipeline→Layoutの逆辺拒否、importの元control blockと依存graph、Graphics／Computeの非所有source拒否、Workの完了／取消後回収を確認した。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL865件中862成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約193件が成功した。変更C++15ファイル／10TUのstrict違反0、CMake303パスとMSBuild／filtersの新4パスの一意登録も成功した。独立レビューで見つかったShader sourceの所有検査と、テスト入力のspan→vectorの明示コピーを修正した。

installed public consumer14targetのbuild／link／CPU実行、Debug／Releaseの実寸法assertとCPU利用、x86_64 cross archive、異artifactのlink拒否が成功した。private Impl／backendは公開しない。arm64のPipelineImplは48 byte・alignment 8、10種を一つずつ置く計画は912 byte・alignment 8となる。

heap／arenaサンプルはいずれも3frame・12submission・2draw、238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回を維持し、PNG3枚が一致した。VUID／sanitizer errorはない。driverのnative allocation全失敗位置、実DeviceLost、Windows／Linux実機、LSan、CPU並行と性能率は未検証である。
