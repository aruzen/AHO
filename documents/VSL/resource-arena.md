# 異種ResourceArena

2026-11-08。[完了計画](arena-completion-plan.md)の順序2。[ResourcePlan](resource-placement-plan.md)に従い、Buffer／Image／SamplerのImplと物理UseStateを同じCPU領域へ構築する。通常VSLと同じconstruct／operationを利用する。

```cpp
#include <VSL/lifetime/resource_arena.hpp>

namespace t = vsl::typed;
namespace s = vsl::storage;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 3> requests{{
    {{1}, s::ResourceKind::BUFFER},
    {{2}, s::ResourceKind::IMAGE},
    {{3}, s::ResourceKind::SAMPLER},
}};
constexpr t::ResourcePlan<3> plan(requests, {t::LifetimeScope::FRAME, 0});
const std::array<l::ResourceInit, 3> inputs{
    vsl::BufferCreateInfo{16, vsl::MemoryType::TRANSFER_SOURCE},
    vsl::ImageCreateInfo{vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST, 1, 1},
    vsl::SamplerCreateInfo{},
};
l::ResourceArena arena(device, plan, inputs);
auto bytes = arena.buffer({1});
auto image = arena.image({2});
auto sampler = arena.sampler({3});
```

## 構築と破棄

入力はexternalを含むplan.size()と同数・同順とし、kindとowned／externalの区別を一致させる。constexpr planも実構築時のABI／領域検査へ通す。型・owner・device等の外部入力検査を済ませ、領域をclaimしてからowned slotを宣言順に構築する。3資源間のnative生成依存はdeviceのみである。descriptor等の資源間依存は後続段階で明示的なDAGへ拡張する。

構築成功の記録に使う領域はclaim前に予約する。後続slotが失敗すると、成功したslotだけを逆順に破棄して自分のclaimを解除する。claim自体の失敗では、既存arenaの使用権を変更しない。最後のarena ownerが解放されるまで領域を再使用できない。

owned実体は領域の先頭＋offsetから直接参照する。内部recordはborrowed pointerを持ち、arena自身への強い所有参照を作らない。外側のownerがdeviceとCPU領域を保持し、native破棄を終えてからそれらを解放する。

## viewと外部資源

buffer()はowning BufferView、image()／sampler()は既存accessorのaliasing handleを返す。ownedの各viewは同じarena control blockを使う。通常操作や保存したcommandは、取得したImplへ直接届く。image／sampler accessorは現APIとの移行adapterで、最終的なref／view APIへの整理は順序12までに行う。

externalでは対応するResourceInitにBufferView／ImageAccessor／SamplerAccessorを渡す。元のImpl・UseState・ownerを維持し、importしたarenaのownerで包み直さない。同じ外部ownerはcontrol block単位で一度だけ保持する。external viewだけを保存しても、import側の無関係なowned資源は保持しない。

部分BufferViewのoffset・sizeはそのまま保持する。非所有の物理Buffer全体を返すborrow_buffer()はlvalueからのみ取得でき、部分externalには利用できない。範囲を維持するbuffer()を使う。借用値はarenaの有効scope内だけで使用する。

外部入力は構築済みnative handle・物理UseState・同一device・有効control blockを要求する。AHO等の追加論理制約を含むTyped外部入力は、後続bundle接続でDescriptorValue／ResourceUseを維持する。このAPIのBufferView／ImageAccessorは物理世代の入口であり、追加の論理制約を推測しない。

DEVICE_LOCAL Bufferも共通backendで構築できる。旧BufferArenaのHOST_VISIBLE限定をこのarenaには適用しない。実際のhost可視性は選択されたメモリtypeに依存する。

## 同期と保持

生成時には使用を登録しない。Typed descriptorや転送・barrier等の記録時に、各Implの共通UseStateを登録する。ownerはarena単位で重複排除し、異なるslotの使用状態は省略しない。完了観測と保持解除の既存の役割分担を維持する。

既存commandのSUBMISSION保持は従来どおり残る。COMPLETIONを選ぶTyped descriptorやBufferView転送経路では、GPU完了／cancel後に記録のpinを回収できる。別に保存したview／commandはその所有期間までarenaを保持する。GPU使用中にslotを移動・再確保しない。

## 残る確保とABI

CPU一括領域はImplとUseStateの本体。arena owner、CPURegion、record／外部依存／構築記録のvector、使用状態vector、Vulkan device memoryは別確保として残る。性能改善率は未測定。

公開plan・初期化info・view等の定義をABI fingerprint／configure依存に追加した。metadataやResourceInitを渡すbinary境界で、slot寸法だけが同じ古いheaderとの混在を認めない。private Implは公開・installしない。

## 検証

Clang20／C++26、macOS arm64、ASan／UBSan、MoltenVK／Vulkan validationで追加GPU9件が成功。異種配置・同型の別slot・通常Bufferとの転送・DEVICE_LOCAL転送・途中失敗後の再利用・claim失敗・外部owner重複排除・部分範囲・移動元・未構築external・Typed computeの37／83読取・wrapper破棄後の利用・完了／取消回収を確認した。

VSL全734件中731成功／既知packed format3 skip、AHO GPU137件・CPU1070件、compile160件が成功。変更C++5ファイル／4TUのstrict違反0。公開ヘッダーだけのinstalled consumerがbuild／linkし、空device拒否も確認した。Debug／Releaseで寸法assertとCPU実行に成功し、同寸法でも異artifactの混在はlink拒否した。

heap／arena両TypedサンプルでUBO更新・resize・pipeline生成1回を維持し、PNG3枚がバイト一致。CMake source path269件とMSBuild新規2path／重複検査も成功した。

Windows／Linux実機、CPU並行操作、device lost、すべてのnative失敗箇所の注入、性能比較は未検証。CPU側の並行操作は利用側で直列化する。LeakSanitizerは無効化し、validation・ASan／UBSan・weak_ptrによる実際の回収・転送結果を併用した。
