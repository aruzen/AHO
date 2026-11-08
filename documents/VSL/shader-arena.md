# Shaderの共通backendとCPU配置

[完了計画](arena-completion-plan.md)の6a。ShaderModuleのVulkan操作、CPU実体の構築、所有と配置を分離し、通常のShaderとResourceArenaから共通処理を利用する。PipelineLayoutのruntime／recipe分離とTyped共有Layoutは6b／6cに残る。

## 構築と配置

`ShaderCreateInfo`はstage、SPIR-V word vector、entry名を所有する。通常のファイル／spanコンストラクタもこの入力から共通backendを使う。lifetimeは指定領域へShaderImplを構築し、operationはheader／entryを検証してShaderModuleを生成する。初期化途中の失敗は構築したCPU実体を破棄し、arenaは成功済みslotを逆順に回収する。

ImplにはModule、保存済みnative Device、Deviceへの借用、owner node、stage、entry、codeを置く。GPU UseStateは付けない。外側のheap ownerまたはarena rootがDeviceを保持する。module破棄の具体処理はoperation、破棄の呼出とCPU実体の回収はlifetimeが担当する。

```cpp
namespace t = vsl::typed;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 2> requests{{
    {{1}, vsl::storage::ResourceKind::SHADER},
    {{2}, vsl::storage::ResourceKind::SHADER}}};
constexpr t::ResourcePlan<2> plan(requests);
const std::array<l::ResourceInit, 2> inputs{
    vsl::ShaderCreateInfo{vsl::ShaderType::VERTEX, vertex_words},
    vsl::ShaderCreateInfo{vsl::ShaderType::FRAGMENT, fragment_words}};
l::ResourceArena arena(device, plan, inputs);
auto vertex = arena.shader({1});
auto fragment = arena.shader({2});
vsl::pipeline_layout::ShaderGroup group("placed", {vertex, fragment});
```

通常の`Shader<type>(device, path/span, name)`は維持する。ShaderAccessorからもname／type／code／get_device／owner_refを取得できる。code()が返すspanはAccessorの所有が続く間の借用で、一時Accessorからの取得は拒否する。code／entryの元入力を変更・破棄しても構築済み実体には影響しない。

arenaのconst ResourceInitからはcode／entryをコピーし、構築処理内ではmoveでImplへ渡す。CPU一括領域はImpl本体だけであり、vector／stringの可変長領域とVulkan内部のメモリは別に確保される。コピーなしや性能率の改善は主張しない。

## 所有とnative生成scratch

owned ShaderAccessorはarenaの正規ownerへaliasする。externalのplan entryにはShaderAccessorを渡し、元Impl／control blockを維持する。importer arenaのgetterは元ownerを返すため、それだけを残しても無関係なimporterのslotを保持しない。heapとarenaのowner nodeは既知のprovenanceを維持し、opaqueなcustom ownerの内部依存は従来どおり呼出側の契約とする。

ShaderGroupは空／非所有Accessorを保存しない。injectionとpipeline生成では同じDevice、live module、単一の既知stage bitを検査する。ERROR_やALL_GRAPHICSのようなtagでmoduleを保持することはできるが、具体的なpipeline stageとしては拒否する。

pointerを含むPipelineShaderStageCreateInfoをShaderImplとCreateInfoから除去した。Graphics／Computeはrecipe.shadersから生成時だけstage情報を組み立て、native呼出が終わるまで配列とShader／entryを保持する。reflectionは保存code／name／stageを使う。Frozenのコピーから元Shader／arena facadeの破棄後にも再生成できる。

6a時点ではCreateInfo／ShaderGroupのshared_ptr<ShaderImpl>保持を維持している。作成済みPipelineもrecipe経由でShader arenaを保持する。GPU完了だけでShader arenaが必ず回収される構成とは扱わない。6bでnative runtimeと再生成recipeを分け、同rootの借用と外部source所有をgraphへ接続する。Device getterのshared_from_thisと公開data_も後続の移行対象である。

## 寸法と互換性

ResourceKind::SHADERはImplLayoutの実size／alignmentを使い、USE_OFFSETはNO_SEPARATE_USEとする。CMakeのcompile-only probe、生成定数、private実定義とslotのstatic_assert、ABI fingerprint、installを同じartifactへ揃える。ShaderImplとCreateInfoのprivate構造が変わるため利用側を再ビルドする。

通常APIの引数とShader::TYPE／nameの呼び方は維持する。privateのshader_stage_create_info／CreateInfo.shader_stagesを読んでいた利用側は、ShaderAccessor::type／name、またはrecipeのShader一覧を使う。native stageのpNameを永続保存する方式へ戻さない。

## 検証

CPU2件・GPU6件を追加した。同型slotのoffset／alignmentとNO_SEPARATE_USE、入力変更後のcode／entry保持、Accessorの寿命、無効header／entry後の領域再利用とimport元の非破棄、null／非所有／別Device／複合stage拒否を確認した。arena facade破棄後のFrozen再生成で実描画とcompute結果を検査した。Shader importだけで固定親の辺が存在することは、逆向きdescriptor更新の拒否と両root回収で確認した。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、最終VSL825件中822成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約175件が成功した。strict21C++／14TU違反0、CMake285パス、MSBuild／filtersの新5パスの登録と重複を検査した。独立レビュー2名の指摘を反映した。

installed public consumer9targetのbuild／link／CPU実行、新ShaderCreateInfoの提供とprivate定義非公開を確認した。Debug／Releaseの実寸法assertとCPU利用、x86_64 cross archive、異artifactのlink拒否も成功した。このarm64構成のShaderImplは88 byte・alignment 8で、追加のUseState領域を含まない。

heap／arenaサンプルは3frame・12submission・2draw・238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回を維持し、PNG3枚が一致した。VUID／sanitizer errorはない。Windows／Linux実機、CPU並行、実DeviceLost、すべてのnative allocation失敗位置、LSan、性能率は未検証である。Native driverの最終メモリ表示をリーク不在の証拠として扱わない。
