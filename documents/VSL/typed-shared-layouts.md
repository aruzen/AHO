# Typed pipelineへの共有DescriptorLayout入力

[完了計画](arena-completion-plan.md)の6c。TypedResourceBundleのshared arenaへ配置済みのLayoutをpipelineへ渡し、pipeline生成ごとのDescriptorLayout再生成を除く。[PipelineLayout runtime／recipe分離](pipeline-layout-arena.md)の所有契約を維持する。

## 公開APIとschema検査

```cpp
namespace t = vsl::typed;
namespace pl = vsl::pipeline_layout;
using Bundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<>>;
Bundle bundle(commands, descriptor_manager, sizes);
auto layouts = bundle.descriptor_layouts();
t::TypedComputePipeline<Resources> pipeline(device, layouts,
    pl::ShaderGroup("shared layouts", {compute_shader}));
// Graphicsもdevice、render_pass、layouts、従来のstage群の順で受け取る。
```

TypedDescriptorLayouts<Resources>は宣言順のLayout alias、必要時の空Layout、sourceのDevice所有を保持する。bundle／SharedState／Frame facadeを保持する新しいownerやcontrol blockは作らない。ResourceSchemaが同じ型の専用constructorだけを用意し、一般のstage injectorにはしない。別schemaの集合はコンパイル時に拒否する。

集合の構築時には型tagだけを信頼せず、各sourceのowner、生存、Device、binding番号、descriptor type／count／stage flags、必要な空Layoutの実構造を検査する。Set importと同じoperationの構造検査を使い、検証ループを複製しない。現在モデル化していないimmutable samplerを持つ定義は受け入れない。

通常VSLから作ったBindingLayout配列も入力できる。

```cpp
t::TypedDescriptorLayouts<Resources> layouts(
    std::array{set_2_layout, set_5_layout}, empty_layout);
auto set_2 = layouts.layout<2>(); // owning alias。元の集合の生存に依存しない。
```

arrayはschemaの宣言順とする。layout<set>()は宣言したsetだけで使用でき、値でownerを返す。一時の集合から呼んでもaliasが領域を保持する。hole_layout()は穴がなければnullopt、あれば正規の空Layoutを返す。追加の空sourceが不要なschemaへ余分なsourceを渡すと拒否する。非空の集合をmoveした後の利用も拒否する。

空schemaの集合はsource／Deviceを保持しない。bundle側にも領域は必要ない。pipelineのDeviceは通常の構築入口で検証する。source付きの集合は同じDeviceのpipelineにだけ使う。

## 配置計画と穴

TypedDescriptorPlanのLayout一覧、Set実数とPool容量は宣言数のままとする。native pipeline layoutのset番号の穴は次の規則で埋める。

1. schema内に宣言した空Setがあれば、そのLayoutをすべての穴へ再利用する。
2. なければshared arenaの末尾へ空DescriptorLayoutを一つだけ追加する。
3. 穴用のSet／Poolは作らず、frame arenaにも穴用のimportを追加しない。

TypedBundlePlan::empty_descriptor_layout()は必要な空LayoutのResourceId、不要ならnulloptを返す。NEEDS_EMPTY_LAYOUTは追加slotが必要かを表す。追加IDも既存descriptor用IDの後ろから取り、externalを含む既存資源IDと衝突させない。

data／宣言Layout／Pool／Setの既存ID・slot・offsetは維持する。arena_planの全要素数／bytesは追加slotの分だけ増える。data-onlyのresource_plan／bytes、宣言数、frame計画は変えない。arena_planを使う利用者は、固定した旧slot数を一つ増える場合に対応させる。空schemaと、すでに空Setを宣言したschemaは増えない。

SET_COUNT分のnative配列は生成時だけ組み立てる。DeviceのmaxBoundDescriptorSetsを確認してから確保・展開し、COUNT／Set数／Pool容量へ混ぜない。たとえば空Set500だけのbundleは小さな配置で作成でき、pipeline生成時にDevice上限で拒否される。

## 操作・所有・完了の経路

bundle.descriptor_layouts()はshared arenaの正規aliasを取得する。専用Typed constructorは末尾のResources{}注入を共有sourceの注入へ置き換え、native DescriptorLayoutを作り直さない。既存のInput／resource／pushのShader検査を維持し、native PipelineLayoutとPipelineには従来と同じbackendを使う。

recipeは解決済みImplを借用し、source ownerをcontrol block単位に一度だけ保持する。ShaderとLayoutが同じarenaのときもsourceを一つにする。Frame facadeはこの所有に含まれない。元bundleを破棄して集合だけを残し、後からpipelineを作れる。

native Pipeline生成後、集合と全recipeを破棄すればsource rootを回収できる。Pipeline runtimeはDescriptorLayoutへの所有を追加しない。元sourceの回収後にも、同じ定義の別bundleをbindingして使用できる。Set／commandの使用状態、記録→submit→完了／cancelの保持は既存の契約を維持する。

集合の通常代入はcopy-and-swapを使う。Deviceと全sourceを公開してから旧ownerを解放し、callbackから不完全な組を見せない。同一オブジェクトの並行変更には呼出側の同期が必要である。

## 互換性と残る移行

従来のTypedGraphics／Compute constructorとHeapStorage bundleは維持する。bundle.descriptor_layouts()は今回ArenaStorageに提供する入口で、HeapStorageのManager割当経路は変更しない。Heap側へ同じhandle共有を広げるときは、指定LayoutからSetを生成する入口と一緒に整理する。

arenaサンプルは新constructorへ移行し、heapサンプルは従来の経路を使う。denseなset0／1なので追加の空slotはなく、CPU配置は1520 byteのままである。

Layout集合のarray／optionalにはowning aliasがあり、Deviceは所有境界でshared_from_thisから取得する。このgetter adapterは起動資源の移行で置き換える。recipe、生成scratch、Source集合、内部vector、Native Pipeline／Layoutのheap所有とVulkan内部の確保は残る。Typedのarena既定化は順序11で行い、速度の改善率は今回測定していない。

## 検証

新CPU1／GPU5件を追加した。型と実構造の不一致、非所有／move後、空schema、宣言emptyの再利用、source handleの同一性、穴の共有、Device上限、frame群の独立回収を確認した。元bundle破棄後のpipeline構築、元source回収後に別bundleでの実compute、ShaderとLayoutが同rootでrecipe ownerが一つになる経路も検査した。既存の12回frame再利用computeは共有constructorへ移行した。

型契約6件で正入力、Compute／Graphicsの別schema、未宣言set、Heap入口、汎用injectorへの混入を検査した。macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL840件中837成功・既知packed format3skip、AHO GPU150件／CPU1070件、既存compile175＋追加6が成功した。strict15C++／9TU違反0、CMake292パス、MSBuild／filters新2パス各1回を検査した。

installed public consumer11 targetのbuild／link／CPU実行、Debug／Release実寸assertとCPU利用、x86_64 cross archive、異artifactのlink拒否、一時recipe借用拒否が成功した。heap／arenaサンプルは3frame／12submission／2draw／238831更新画素、resize1720×1080→2000×1360、pipeline生成1回、PNG3枚の完全一致を維持した。VUID／sanitizer errorはない。

Windows／Linux実機、CPU並行、実DeviceLost、全native allocation失敗位置、LSan、性能率は未検証である。次は順序7のRenderPass／Pipeline／Framebufferへ展開する。
