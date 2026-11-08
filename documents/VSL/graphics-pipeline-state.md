# native構築から独立したGraphicsPipelineState

[完了計画](arena-completion-plan.md)の7b前半。Pipelineをarenaへ配置する前に、fixed-functionの生成入力をDevice／native Layout／Shader／Passから分ける。Pipeline backendと配置は次の単位であり、この変更だけで7bを完了とはしない。

```cpp
namespace pl = vsl::pipeline_layout;
vsl::GraphicsPipelineState state(
    Input{}, pl::ColorBlend{}, pl::InputAssembly{},
    pl::Rasterization{}, pl::Multisample{}, pl::DynamicState{},
    vsl::Viewport(0, 0, width, height),
    vsl::Scissor(0, 0, width, height));

auto independent = state; // 配列も独立したコピー
pl::Rasterization{pl::CullingMode::NONE}.injection(independent.edit());

// 既存入口にも同じ入力を渡せる。stateの構築ではnative Layoutを生成しない。
vsl::PipelineLayout layout(device, shader_group, state, resources, pushes);
auto pipeline = vsl::GraphicsPipeline(std::move(layout).freeze(), pass);
```

## 値・借用・所有

公開GraphicsPipelineStateはprivateなGraphicsPipelineStateDataをunique_ptrで所有する値型である。Device／native resource／source ownerを含まず、CPUだけで構築できる。copyは可変長配列も含めて独立し、moveは保存先のpointerを移す。move済み値のdefinition／editはlogic_errorで拒否し、別の値の代入で再利用できる。

CPUの定義用本体とvector領域の個別確保は残る。この入力値をCPU Implの一括配置やVulkan device memoryと混同しない。生成済Pipelineが定義を保持する構成にはせず、後続backendは生成呼出し中の借用として読む。

`edit()`はlvalueのstateからだけ、Device／ownerを持たないGraphicsPipelineStateEditRefを返す。`definition()`はconstを含むrvalueから取得できない。どちらも元の値が生存し、該当操作中に変更されないことが利用契約となる。ownerを保持するviewとは異なる。公開headerへprivate定義とVulkan SDKの型を公開しない。

固定stageとTypedVertexInput、Viewport／Scissor、IDPickingはEditRefへの共通注入本体を使う。従来のPipelineLayoutEditAccessor overloadはその本体へ委譲する。ShaderGroup／ResourceBinding／PushConstantsはDeviceや資源への依存を持つため、state-only constructorのconceptで拒否する。入力順と、VertexInputの失敗時不変の保証を維持する。

未注入のstateは従来の空のfixed fieldsを保つ。たとえばColorBlend stageの既定1attachmentを自動で足さない。Stage値をそのまま各memberの既定値として置く方式では、この違いが出るため採用しない。

## 既存LayoutとTypedへの接続

PipelineRecipeは同じprivate GraphicsPipelineStateDataをinlineで保持する。state値のための新しい個別確保を既存Layoutへ追加しない。Runtime／recipe／scratchの分離と、Shader／DescriptorLayout sourceの所有は維持する。

`state.injection(layout)`はfixed state全体をsnapshotとして置換する。まず配列を含む一時copyを構築し、その成功後にnothrow swapで公開する。Shader、binding layouts、push ranges、source owner、native Layoutを変更しない。移動済みsourceやcopy失敗では元Layoutを保つ。前のstageへの追加を意図する場合は、各stageを`layout.edit_state()`またはstateの`edit()`へ注入する。

TypedGraphicsPipelineにも通常のstage引数として渡せる。Typed側の`Input{}`は引き続き末尾で注入し、先行stateの頂点定義でTyped契約を上書きしない。実Shaderのvertex／resource／push検証は既存経路を維持し、無検査のTyped変換を追加しない。

Layoutとstateの接続adapterはvulkan/pipeline_layout.cppに置く。operationのstate実装は値と借用を扱い、公開Layout facadeに依存しない。既存のPipelineScratchは同じデータ型の配列からpointersを生成し、生成呼出し中にだけ使う。

## 互換性と次の単位

通常のstage注入、PipelineLayout／Frozen、Typedの公開生成引数は維持する。private recipeのfixed fieldsは`recipe().graphics_state`へ移す。private定義の利用側は、たとえば`recipe().rasterization`を`recipe().graphics_state.rasterization`へ更新する。生成headerのfingerprintも公開入力とprivate定義の変更に追従し、利用側を再ビルドする。

次は解決済みLayout／Pass／Shader参照とこのstateを受ける共通Pipeline backendである。Heap ownerにはLayoutへの固定依存を列挙し、arenaは独立資源→Set／PipelineLayout→Pipelineの順で構築して逆順に戻す。直接Shader／Passは生成時source、Layoutは永続依存として分ける。Recipeの取得だけを目的とするheap native Layoutを作らない。

## 検証

CPU4件・GPU2件・compile7件を追加した。Deviceなしでの全固定stageとTyped頂点metadataの解決、未注入default、deep copy／代入／move、無効VertexInputの失敗時不変、Typedの末尾Input注入、snapshotのsource非変更と入力からの独立を検査した。public header単独include、Shader／descriptor／pushのstate-only入力拒否、一時state／Layoutからの借用拒否も確認する。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL855件中852成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約192件が成功した。strict43C++／28TUと、レビュー後に変更した3ファイルの再検査は違反0。CMake299パスとMSBuild／filtersの新2パスの一意登録も確認した。

heap／arenaサンプルは3frame・12submission・2draw、238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回を保ち、PNG3枚が一致した。VUID／sanitizer errorはない。native資源の配置、Windows／Linux実機、CPU並行、LSan、全allocation失敗位置、性能率はこの単位の検証範囲外とする。

installed public consumer13targetのbuild／link／CPU実行、private定義の非公開、新headerの提供を確認した。Debug／Releaseの寸法assertとCPU利用、x86_64 cross archive、異artifactのlink拒否、installed headerでの一時state編集拒否も成功した。配置寸法は既存9種864 byte・alignment 8を維持する。独立レビュー2名の指摘を反映した。
