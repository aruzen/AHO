# 異種資源のCPU配置計画

2026-11-08。[完了計画](arena-completion-plan.md)の順序1。Buffer／Image／Samplerの不透明なImpl型から正確な寸法を取得し、Typed側で配置を計算する。native生成とarena所有への接続は順序2で行う。順序5bで[Layout／Pool／Setの寸法と配置](descriptor-arena.md)を追加した。

## 寸法とartifact

`storage::ImplLayout<impl::BufferImpl／ImageImpl／SamplerImpl>`が実体のSIZE／ALIGNMENTを公開する。Implの完全定義はprivateのまま、既存のcompile-only CMake probeで対象compiler・architecture・configurationのsizeof／alignofを取得する。

`ResourceSlotLayout<Impl>`はImpl先頭と物理UseStateのoffset、slot全体のsize／alignmentを計算する。VSLの翻訳単位で、実際のBufferSlotStorage／ImageSlotStorage／SamplerSlotStorageとのsize・alignment・両field offsetをstatic assertする。将来Device等を加える際は、その資源に必要な状態に合わせてslotを定義し、UseStateを不要な型へ押し付けない。

生成ヘッダーとbinaryの対応を表すRESOURCE_LAYOUT_ABIは、対象Impl寸法・使用状態・private slotヘッダー・Vulkan C／C++ヘッダー・compiler・対象環境・構成・flags等から生成する。runtime plan構築／領域検査で対応symbolを要求する。constexprでの純計算にはbinaryを要求しない。ResourceArenaも実構築入口で必ずruntime検査を行う。

BufferLayout、既存の生成macro／ヘッダー名、既存Bufferのartifact guardは互換入口として残す。RESOURCE_LAYOUT_ABIは同じ生成artifactを表す。生成方式は引き続きVSL全体で必須。生成ヘッダーは構成ごとにinstallされ、consumerはCMake targetから選択情報を受け取る。

## ResourceIdからslotへの配置

```cpp
#include <VSL/lifetime/resource_plan.hpp>

namespace t = vsl::typed;
namespace s = vsl::storage;
constexpr std::array<t::ResourceRequest, 4> resources{{
    {{10}, s::ResourceKind::BUFFER},
    {{20}, s::ResourceKind::IMAGE},
    {{30}, s::ResourceKind::BUFFER},
    {{40}, s::ResourceKind::SAMPLER, true}, // 外部資源
}};
constexpr t::ResourcePlan<8> plan(resources, {t::LifetimeScope::FRAME, 2});
static_assert(plan.slots() == 3);
static_assert(plan.placement({10}).offset != plan.placement({30}).offset);
auto bytes = plan.bytes();
auto alignment = plan.alignment();
plan.require_region(bytes, alignment);
```

一つのResourceRequestが一つのImplを表す。capacityは宣言数の上限で、実個数は入力spanの長さから決まる。planは実入力分のmetadataをコピーして保持し、入力spanを借用しない。payloadの要素数・画像extent・GPUメモリ容量とは別の計算である。

ResourceIdはplan内で一意。同じImpl型を複数宣言してもIDとslotは異なる。複数bindingが同じ資源を使う場合は上位のbinding mapから同じResourceIdを参照する。ResourceId、BindingKey、物理slotを混ぜない。既存ResourceId／LifetimeScope／LifetimeGroupはnamespaceと型identityを保って共通ヘッダーへ移した。

planは一つのLifetimeGroupのowned領域を表す。SHAREDのinstanceは0、FRAME／TRANSIENTのinstanceは利用側が識別する。外部資源は元の寿命群と所有を保ち、planの群へ移動しない。グループをまたぐ識別はLifetimeGroupとResourceIdを併用する。

externalの項目はslotとoffsetの両方がNO_SLOTとなり、CPU領域を消費しない。空plan／externalのみのplanは0 byte・alignment 1。各owned slotを要求alignmentへ丸め、領域末尾も全体alignmentへ丸める。

## 検査と所有

重複ID、未知kind／scope、SHAREDの非zero instance、capacity超過、size加算とalignmentのoverflow、領域の容量／宣言alignment不足を拒否する。実アドレスのalignmentと領域の排他使用はArenaMemory／後続ResourceArenaが検査する。

`placement(id)`は値を返す。`placements()`は実入力数だけのreadonly spanで、lvalueからのみ取得できる。一時planからの借用をconst rvalueも含め拒否する。plan自体はCPU資源やGPU資源を所有せず、構築済み／未構築slotの状態管理も行わない。

## 検証

Clang20／C++26、macOS arm64、ASan／UBSanでCPU配置5件、Typed compile158件（新規7件、うち負例6件）を確認。VSLは725件中722成功／既知packed format3 skip、AHO GPU137件・CPU1070件が成功。変更C++7ファイル／4TUのstrict違反0。

private実定義とのstatic assert、公開ヘッダーだけのconstexpr／runtime利用、install consumerのbuild／link／実行を確認した。3種を一つずつ配置したCPU領域はこの構成で264 byte・alignment 8。payload／GPUメモリ量ではない。

Debug／Releaseの両構成で実寸法検査とCPU実行に成功。両構成の寸法は同じでも異artifactのheader／binary混在はlink guardで拒否した。x86_64向けobject／static library生成にも成功した（実行なし）。Windows／Linux実機、性能計測は未実施。GPU回帰はMoltenVK／Vulkan validationを使用し、LSanは無効化した。

## 次の接続

ResourceArenaはこのplanとcreate info／外部viewを受け取り、入力indexとowned slotを区別して構築する。runtimeのABI／容量／alignment検査を通し、owned slotの内部参照とarena全体の外部所有を分ける。失敗時は構築済みslotだけを逆順に回収する。
