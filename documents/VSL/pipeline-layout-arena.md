# PipelineLayoutのruntime・recipeとCPU配置

[完了計画](arena-completion-plan.md)の6b。PipelineLayoutのnative実体、再生成用recipe、生成時scratchを分ける。通常のVSLとResourceArenaは同じoperation／lifetime backendを利用する。Typedのschema付き共有Layout集合は次の6cで接続する。

## 4責務と所有

- storage：生成した実size／alignmentからPipelineLayoutImplのslotを計算する。ImplはGPU UseStateを持たない。
- operation：解決済みのDevice／DescriptorLayoutから検証、push範囲の正規化、native生成・破棄を行う。
- lifetime：指定slotのCPU構築・破棄、Deviceを保持するheap owner／arena root、途中失敗の回収を扱う。
- sync：既存Completion／SubmissionWorkが完了を観測する。記録の所有保持と解放はlifetimeへ接続する。

PipelineLayoutImplにはraw Device、native Device／Layout、正規owner node、canonical push ranges、descriptorの構造signatureだけを置く。runtime rootはrecipeを所有しない。heapではDeviceとslotだけを持つ独立ownerを使う。DescriptorSetLayoutはnative Layoutの生成呼出し中だけ必要で、runtimeは構造値をコピーする。Pipelineがnative Layoutを保持する既存契約は維持する。

PipelineRecipeはruntime rootの外に置く。編集用Layout／Frozen facadeがruntimeとrecipeの両方を保持し、PipelineImplはruntimeだけを保持する。recipe内のShader／DescriptorLayoutは解決済みの借用pointerとし、sourcesで正規ownerをcontrol block単位に一度だけ保持する。descriptorの置換では不要なsourceを外し、残るShaderのowner indexを更新する。recipe ownerをarena rootの依存へ渡すAPIは作らない。同じarenaへShaderや明示import資源とruntimeをまとめた場合は、そのarenaの一括寿命が優先する。

標準配列は型付きvectorで所有し、従来のmap<string, any>を使わない。vertex input、viewport、dynamic state、blend配列へのnative pointerはPipelineScratchで生成呼出し中だけ組み立てる。scalarのみを使う固定機能設定のnative構造体ではpNext等をnullのままにする。Shader stageとentry pointerも生成呼出し中だけ借用する。

## 通常のVSLとarena

通常の構築方法は維持する。

```cpp
namespace pl = vsl::pipeline_layout;
vsl::PipelineLayout editable(device, shader_group, pl::ResourceBinding{{binding}},
    pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
auto frozen = std::move(editable).freeze();
vsl::ComputePipeline pipeline(frozen);
```

PipelineLayoutInitはset番号順のDescriptorLayout sourceとpush範囲を受け取る。ResourceIdは明示arena資源、直接渡すBindingLayoutは構築呼出し中だけのsourceとして区別する。ID参照では同じDescriptorLayoutを複数setから使える。set番号のholeを詰めず、空DescriptorLayoutを明示して埋める。

```cpp
namespace t = vsl::typed;
namespace l = vsl::lifetime;
using enum vsl::storage::ResourceKind;
constexpr std::array<t::ResourceRequest, 2> requests{{
    {{1}, PIPELINE_LAYOUT}, {{2}, DESCRIPTOR_LAYOUT}}};
constexpr t::ResourcePlan<2> plan(requests);
const std::array<l::ResourceInit, 2> inputs{
    l::PipelineLayoutInit{{t::ResourceId{2}}, {{vsl::ShaderFlag::COMPUTE, 0, 4}}},
    vsl::DescriptorLayoutCreateInfo{{{0, vsl::graphic_resource::Type::STORAGE_BUFFER,
                                    vsl::ShaderFlag::COMPUTE, 1}}}};
l::ResourceArena arena(device, plan, inputs);
auto runtime = arena.pipeline_layout({1});
auto recipe = vsl::PipelineLayout::FromRuntime(runtime, shader_group,
    pl::ResourceBinding{{arena.descriptor_layout({2})}},
    pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
vsl::ComputePipeline pipeline(std::move(recipe).freeze());
```

arenaのgetterとruntime()はrecipeを持たないowning accessorを返す。FromRuntimeはnative再生成をせず、別に構成したrecipeとruntimeのdescriptor構造・canonical push範囲が正確に一致することを検査する。対応済みdescriptorにはimmutable sampler／layout flagsを持たせていない。将来それらを追加するときはsignatureを拡張する。bindingの宣言順だけが異なる同一構造は受け入れる。異なるbinding／count／stage／pushは拒否する。runtimeだけでPipelineを作ろうとするとrecipe不在として拒否する。

ResourceArenaは独立資源を先に、Set／PipelineLayoutを後に構築する。宣言順やoffset順に依存せず、実構築順を記録して逆順に回収する。未定義IDや型の違う依存はpreflightで拒否する。native生成失敗時には当該CPU実体を破棄し、それ以前の構築済みslotだけをrollbackする。regionは再利用できる。external runtimeは元Impl／ownerを保ち、importerや渡されたrecipeをgetterのownerへ混ぜない。

## 編集・記録・回収

copy()はrecipeの値とnative Layoutを独立構築する。add()は新しい組を構築してから全所有値をswapする。失敗すると元の組を維持する。通常代入もcopy-and-swapとし、旧ownerのcallbackには公開済みの一貫した組を見せる。

freeze()は編集元を消費し、runtimeを再生成しない。編集可能なrecipeの別名が残る場合だけrecipeをコピーする。これによりarena runtimeをfreeze時にheapへ移し替えない。recipe()／edit_recipe()の参照はfacadeの生存中だけ有効で、一時オブジェクトからの取得を拒否する。編集用stageの直接injectionは既存の組立用入口として残すが、binding／pushを変えたrecipeはnativeと一致しなくなる。Pipeline生成で拒否し、add()／copy()でnativeを再構築する。

保存PushConstantは構築時にruntime()へ縮約する。Typed pushはPipelineImplのruntimeに保存したcanonical rangesで記録し、reflectionやrecipe解決を行わない。全Layout／Frozen／ShaderGroupを破棄すれば、Pipelineや保存PushConstantを残しても別arena／heapのShader sourceを回収できる。Frozenを残す場合はsourceを保持して再生成できる。

記録時のPipeline／PushConstantは既存のSUBMISSION保持を維持する。完了観測だけで記録済みcommand bufferの依存を外さず、同slotの再記録・Work置換まで保持する。cancelはnative記録をresetし、PhaseがWorkを解放すると回収する。Completion結果だけを残しても古いWorkの回収を妨げない。COMPLETION保持への一律変更は行わない。

## 配置情報と残る確保

CMakeのtarget向けcompile-only probe、生成定数、private Impl／slotとのstatic_assert、構成別ABI fingerprint、installed public headerを拡張する。公開ヘッダーへImplの完全定義を公開しない。

一括領域はPipelineLayoutImpl本体だけである。push ranges／signatureのvector、recipeとsource vector、native生成scratch、PipelineImpl、ShaderGroup、DeviceとVulkan内部の確保は別に残る。public data_／shared_ptr<Impl>、Device getterのshared_from_thisは後続の移行用入口である。編集facadeは読み取り用と編集用で同じrecipeへのshared参照を持つが、control blockを追加していない。これも公開入口の整理時に見直す。CPU参照段数とsourceの保持は整理したが、速度や確保数の改善率は未測定である。

## 検証

CPU2件／GPU7件を追加し、関連16件で同型slot、後置ID、exact size／alignment、失敗後のregion再利用、runtime import、同構造・別handleと異なるbinding宣言順の照合、binding／push不一致、recipe sourceの置換、copy／freeze／追加失敗、copy／move代入の再入を確認した。Shaderと未使用DescriptorLayoutのowner失効後にも、残したPipeline／PushConstantからcompute結果を取得し、完了後のslot再利用とcancel後の回収を検査した。直接sourceのnull／非所有は構築前に拒否する。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、最終VSL834件中831成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約175件が成功した。strictは変更51C++／41TUを検査し、指摘6件の修正後に対象5ファイル／3TUを再検査して違反0。CMake290パス、MSBuild／filtersの新5パス各1回登録を検査した。独立レビュー3名の指摘を反映した。

installed public consumer10 targetのbuild／link／CPU実行、private backend非公開、public入力の提供と一時edit_recipe借用の実compile拒否を確認した。Debug／Releaseの実寸法assertとCPU利用、x86_64 cross archive、Debug headerとRelease archiveのlink拒否も成功した。このarm64構成のImplは80 byte／alignment8で、8種類のmixed領域は752 byte／alignment8となった。

heap／arenaサンプルは3frame・12submission・2draw・238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回、PNG3枚の完全一致を維持した。VUID／sanitizer errorはない。Windows／Linux実機、CPU並行、実DeviceLost、すべてのnative allocation失敗位置、LSan、性能率は未検証である。
