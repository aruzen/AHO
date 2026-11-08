# Descriptorのarena配置

[完了計画](arena-completion-plan.md)の5b後半。[共通backend](descriptor-backend.md)をResourceArenaへ接続し、Buffer／Image／SamplerとLayout／Pool／Setを同じCPU領域へ配置できるようにした。操作は解決済みImpl／native Deviceを直接使い、所有を選ぶ処理はlifetime側に置く。

## 通常VSLとTypedからの利用

通常のManager／BindingLayout／Pool APIは同じ構築・操作backendを使う。heapでの外側ownerは引き続き個別に確保する。

```cpp
vsl::GraphicResourceManager manager(device);
vsl::graphic_resource::BindingLayout layout(
    device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE);
auto [pool, resources] = manager.make(std::vector{layout});
resources.front().update(&uniform_buffer);
```

arenaでは資源IDと配置先をplanで決め、SetのPool／Layout依存を初期化情報で指定する。set／binding番号は資源IDに含めない。同じImpl型を複数slotへ配置でき、配置順と構築順は独立する。

```cpp
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/utils/typed_resources.hpp>

namespace t = vsl::typed;
namespace s = vsl::storage;
namespace l = vsl::lifetime;
using Set = t::DescriptorSet<0,
    t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
using UniformView = t::TypedResourceBufferView<
    std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
constexpr std::array<t::ResourceRequest, 4> requests{{
    {{4}, s::ResourceKind::DESCRIPTOR_SET},
    {{1}, s::ResourceKind::BUFFER},
    {{2}, s::ResourceKind::DESCRIPTOR_LAYOUT},
    {{3}, s::ResourceKind::DESCRIPTOR_POOL},
}};
constexpr t::ResourcePlan<4> plan(requests);
const std::array<l::ResourceInit, 4> inputs{
    l::DescriptorSetInit{{3}, {2}},
    vsl::BufferCreateInfo{4, vsl::MemoryType::UNIFORM_BUFFER},
    vsl::DescriptorLayoutCreateInfo{{Set::POINTS.begin(), Set::POINTS.end()}},
    vsl::DescriptorPoolCreateInfo{
        {{vsl::graphic_resource::Type::UNIFORM_BUFFER, 1}}, 1},
};
l::ResourceArena arena(device, plan, inputs);
auto set = t::TypedDescriptorSet<Set>::FromResource(
    device, arena.descriptor_set({4}), vsl::ResourceRetention::COMPLETION);
set.write<0>(UniformView::FromView(arena.buffer({1}), 1));
// 既存のTypedPipelineと同じ経路で記録できる。
// phase << pipeline.bind_resources(set).dispatch(1);
```

`FromResource`は所有するSetとDevice、同一Device、binding番号・type・amount・stageを検査する。native宣言順が異なっていても同じschemaなら利用できる。control blockを持たないaliasは拒否する。`write<binding>`はbinding番号で指定する。移行用の`ResourceUseGroup::replace(index, ...)`は従来どおりnative Layoutの宣言順ordinalであり、schemaの`BindingIndex`とは限らない。

## 配置と構築

`ImplLayout`の対象にResourceBindingLayoutImpl／GraphicResourcePoolImpl／GraphicResourceImplを追加した。対象compiler・architecture・構成・flagsで実定義からsize／alignmentを生成し、binary側のstatic assertとartifact symbolで一致を検査する。生成情報はCMake targetとともにinstallする。private Implはinstallしない。

Layout／Pool slotはImplだけ、Set slotも内蔵UseStateを含むImplだけを配置する。`USE_OFFSET == NO_SEPARATE_USE`は追加の物理状態がないことを表す。Set内の状態のoffsetを公開しない。既存Buffer／Image／Samplerの後置UseStateは維持する。

構築前に型・依存ID・Device・外部ownerを検査し、CPU領域をclaimする。非Setを構築してからSetを構築する。失敗すると成功した実体だけを実構築順の逆順で回収し、claimを解除する。Setのnative割当後に次のSetが容量不足となる場合も、先行Setをfreeして容量を返却する。

公開前に全Setを所属Poolのweak registryへ登録する。importしたPoolも対象とする。必要なregistry容量を先に予約し、登録中の確保失敗を避ける。明示的なPool破棄は各Setのidleを検査し、使用中なら拒否する。破棄可能ならSet handleとbinding metadataを無効化する。

## 所有と同期

arena rootはDevice・CPU領域・固定した外部依存を所有する。内部のSet→Pool／Layoutとbinding→同root資源は借用にする。`DescriptorOwnerContext`はweak selfと固定依存表の参照を持ち、writeと移行用replaceの両入口で同じ所有正規化を行う。同rootまたは宣言済み外部owner以外を指定すると、native更新前に拒否して旧bindingを維持する。

独立した論理制約には`LogicalUseState`を用意した。外部ownerから作れず、他の資源を所有しない状態を自分で生成する。構築時の末尾引数に宣言すると、元のtokenがなくなってもarenaが状態を保持できる。

```cpp
const std::array dependencies{l::LogicalUseState{}};
l::ResourceArena arena(device, plan, inputs,
    s::ArenaMemory(plan.bytes(), plan.alignment()), dependencies);
auto value = UniformView::FromView(
    arena.buffer({1}), 1, 0, dependencies.front().resource_use());
```

任意の`shared_ptr<void>`を依存として追加する入口は提供しない。例えばheap Setを新arenaの依存とし、そのSetへ同arenaのBufferをbindすると相互所有になるためである。可変依存の管理root間の循環検査は5cで扱う。低水準の公開aliasや`data_`を使い、owner内部へ隠した循環まで検出する契約ではない。

各owning facadeは元arenaのcontrol blockを共有する。外部Buffer／Image／Sampler／Layout／Poolのgetterは元の正規ownerを使い、import側rootで包み直さない。`get_pool()`もSet側のownerを保持しない。外部Setのimportは今回未対応。

command作成時はowning Setを保持し、記録時にSet・物理資源・論理制約の共通状態を登録する。同root ownerは記録の保持一覧で一度だけ保持し、各slotの状態は個別にmarkする。同期側のCompletionObservationが完了／取消を観測し、lifetime側のworkが保持を解放する。COMPLETION経路では保存結果だけでarenaを延命しない。保存したview／commandが別にあれば、その所有は続く。SUBMISSIONのsnapshot保持は既存契約として残す。

固定依存はbindingから外れてもroot破棄まで保持する。shared資源と各frameの資源をまとめず、寿命群ごとにarenaを分ける。Typed bundleの自動展開と可変依存への接続は5cで行う。

## Pool容量と互換性

PoolはmaxSetsとtype別descriptor容量を追跡し、native割当前に超過を拒否する。実装が宣言容量を超えて割当可能な場合にも依存しない。free成功時に元のallocation Layoutから使用数を返却し、更新用metadataが変更されても計数を維持する。失敗時は既存Setと計数を変えない。実際のnative free失敗は未検証で、失敗した分の容量は返却しない。

0件のnative bindingは予約領域として許可し、容量とcomplete検査から除外する。書き込みは拒否する。Typed schemaの0件配列禁止は維持する。[Khronosのbinding契約](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorSetLayoutBinding.html)

既存の通常VSLとTyped constructorは維持する。新しいFromResourceとResourceArena getterは追加APIである。Poolの宣言容量超過は明確な失敗へ変わる。CPU並行操作は引き続き利用側で直列化する。

## 対象外の確保と次の段階

一括領域はCPU Impl本体と物理状態だけを含む。Impl内部のmap／vector、binding要素、weak registry、quota計算scratch、root／CPURegion、Typed ResourceUseGroup、通常heap ownerとVulkan device memoryの確保は残る。性能率は未測定。

5cではTyped schemaから宣言Layout数、shared／frame別の実Set数、type別容量を求める。疎set番号のholeをSet数へ含めず、寿命群ごとのPoolとarenaへ配置する。可変の外部依存を扱う前に、管理root間の循環と更新のcommit契約を実装する。shader／pipeline等への展開はその後に行う。

## 検証

macOS arm64／Clang20／C++26、ASan・UBSan、MoltenVK・Vulkan validationで、新GPU12件とCPU2件を追加した。同型別slot、依存順構築、途中失敗と再利用、固定依存と未知owner拒否、owning facade破棄後の記録、共通状態、完了／取消後のweak失効、外部Pool無効化、逆順binding、実sampling／readback、容量再利用と0件予約を確認した。

VSL789件中786成功・既知packed format3skip、AHO GPU140件・CPU1070件、compile契約175件成功。変更C++21ファイル／14TUのstrict違反は0。CMake276パス、MSBuild／filtersの新規2パスと重複も検査した。

公開ヘッダーだけのinstalled consumer4targetがbuild／link／CPU実行でき、新LogicalUseStateの提供とprivate Impl非公開を確認した。Debug／Releaseの実定義assertとCPU利用、同寸法でも異artifactの混在時のlink拒否、x86_64 cross archiveも成功した。5b時点のarm64構成で6種を一つずつ配置したCPU領域は536 byte・alignment 8だった。[所有依存graph](owner-dependencies.md)のnode pointer追加後は584 byteとなる。GPUメモリ量ではない。

heap／arena両サンプルは3frame・12submission・2draw・238831更新画素・resizeとpipeline生成1回を維持し、PNG3枚が一致した。VUID／ASan／UBSan errorはなかった。AHOの非対応整数format blendingを意図的に拒否する既存テストでは、期待どおりdriver warningが出る。

LSan、Windows／Linux実機、CPU並行操作、実DeviceLost、すべてのnative失敗位置と性能比較は未検証。GPU driverの最終メモリ表示をリーク不在の証拠には用いない。
