# Typed descriptorの論理計画

[完了計画](arena-completion-plan.md)の順序5cの最初の単位。`TypedDescriptorPlan<AllocationSchema>`はschemaからLayout宣言、寿命群ごとのSet、Poolの型別容量を求める。ResourceId／slot／offsetを決める配置計画と、Vulkan生成情報を組み立てるruntime scratchから分離する。

```cpp
#include <VSL/lifetime/typed_descriptor_plan.hpp>

namespace t = vsl::typed;
using Local = t::DescriptorSet<2,
    t::UniformBinding<0, float, vsl::ShaderFlag::COMPUTE, 2>,
    t::StorageBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
using Shared = t::DescriptorSet<9,
    t::UniformBinding<0, float, vsl::ShaderFlag::COMPUTE>>;
using Schema = t::ResourceAllocationSchema<t::ResourceSchema<Local, Shared>,
    t::BufferAllocation<2, 0, t::FixedElements<1>, t::ResourceScope::SHARED>,
    t::BufferAllocation<2, 3>,
    t::BufferAllocation<9, 0, t::FixedElements<1>, t::ResourceScope::SHARED>>;
constexpr t::TypedDescriptorPlan<Schema> plan(3);
static_assert(plan.Layouts().size() == 2);
static_assert(plan.total_sets() == 4 && plan.total_pools() == 4);
static_assert(plan.total_descriptors() == 10);
static_assert(plan.pool(t::ResourceScope::PER_FRAME).max_sets == 1);
```

Layoutは宣言したsetの数だけ保持し、bindingはschemaの静的`POINTS`を参照する。set番号のholeをLayout／Setに含めない。pipelineが必要とするempty hole Layoutは順序6で扱う。明示的に宣言した空Setは、空schemaと区別してLayout／Set／Poolを必要とする。

Setの寿命群は既存`SetIsPerFrame`に合わせる。同じSet内に一つでもper-frame値があれば、全bindingを含むSetをframeごとに一つ作る。shared値を参照するbindingも、そのframe Setのdescriptor容量として数える。shared Setは全frameで一つ。PoolはSetを持つ寿命群ごとに一つで、frame群の実Pool数はframe数になる。

Pool容量はdescriptor typeごとのbinding amountを合算する。descriptor配列は要素数分を数え、combined image samplerは一つのdescriptor typeとして数える。物理資源IDを共有してもbinding側のdescriptor数を減らさない。外部資源もSetのdescriptor容量を使用する。

`sets(scope)`はset番号とLayout宣言ordinalの一覧、`pool(scope)`はmaxSetsとtype別容量を返す。内部の固定配列は宣言Set数／binding数で確保し、frame数やdescriptor配列の要素数に比例して拡大しない。0frame、未知scope、Pool単位のuint32上限と全体集計のsize_t overflowを拒否する。

`TypedBundlePlan::descriptors()`から同じ計画を参照できる。内部spanを返す`sets`／`pool`と、この参照入口はlvalue限定にする。`Layouts()`はschemaの静的情報を返すため計画オブジェクトの寿命に依存しない。既存`resource_plan()`／`resources()`／CPU bytesは、今回の段階では従来のBuffer／Image／Sampler配置の意味を維持する。

外側の`TypedResourceBundle::placement_plan()`もlvalue限定にした。一時Bundleから参照を取り出して下位のrvalue拒否を迂回することを防ぐ。Bundleを変数へ保存してから配置計画を参照する。計画metadata自体はCPU一括領域の外で保持する。

Vulkan生成用のvector／mapをconstexpr計画へ保存しない。次の接続で、このmetadataからruntime scratchを作り、既存の資源IDと衝突しないLayout／Pool／Set IDを付与する。shared arenaへLayoutとshared Set／Pool、各frame arenaへframe Set／Poolを配置し、shared Layout／payloadは元の正規ownerでimportする。

この論理計画だけを導入した段階では、Typed bundle内のdescriptor実体は既存Manager経路にあった。後続の[所有依存graph](owner-dependencies.md)と[descriptor実配置](typed-descriptor-placement.md)で、追加ResourceUseと外部世代を保持したままarenaへ接続した。data-onlyのresource_plan／bytesは意味を維持し、descriptorを含む領域にはarena_planを使う。

CPU6件を追加し、constexpr計算、疎set番号、明示empty Set、空schema、shared／frame別の正確な容量、外部画像、配列、巨大frame数、native容量／全体overflow、一時オブジェクトからの借用拒否を確認した。既存ResourceAllocationPlanのdescriptor_sets／descriptorsとの一致、物理IDを共有してもdescriptor容量が減らないことも検査した。関連する既存計画テストを含む21件が成功した。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL795件中792成功・既知packed format3skip、AHO GPU140件、compile契約175件成功。変更C++4ファイル／3TUのstrict違反0、CMake277パスとMSBuild新headerを検査した。公開ヘッダーだけのinstalled consumer5targetがbuild／link／CPU実行し、新ヘッダーの提供も確認した。

heap／arena両サンプルは3frame・12submission・2draw、UBO更新とresize、pipeline生成1回を維持し、PNG3枚が一致した。VUID／ASan／UBSan errorはなかった。今回はImpl寸法や生成方式を変更していないため、構成別／cross／異artifact検査は5bの結果を引き継ぐ。Windows／Linux実機、CPU並行、DeviceLost、性能率は未検証。
