# 所有参照とroot間の依存

[完了計画](arena-completion-plan.md)の5cで、Typed descriptorの実配置に先立つ所有境界を接続する。lifetimeの`OwnedRef<T>`が所有者と依存先の出所を維持し、rootに埋め込んだ`DependencyNode`が更新時の循環を検査する。syncの使用状態と完了観測にはgraphを持たせない。

## 所有する値と借用するnode

`OwnedRef<T>`は`shared_ptr<T>`と、その所有者内のnodeへの借用を一体で持つ。`OwnerRef`はvoid版である。ResourceUseのstate／owner／constraint_state／constraint_ownerはそれぞれ独立したOwnedRefとし、物理世代・論理状態・追加の資源所有をまとめない。DescriptorValueのBuffer／Image／SamplerとBufferViewも出所を維持する。

代入はreplacementとpair全体のswapで行い、旧ownerの解放callbackが呼ばれる時には新しいpointer／nodeが公開済みになる。resetも同じ順序とする。move元のnodeは空になる。aliasと型変換は元control blockとnodeを維持する。状態の一致は従来どおりState*、所有者の一致はcontrol blockで判定する。stored pointerがnullでもcontrol blockがあれば所有者として扱う。

descriptor metadataはstate／constraint_stateのnull aliasも所有として保持し、raw stateの登録だけ省く。nonnull pointer・use_countが0の非所有stateは保存せず拒否する。ResourceUse::recordは従来どおり明示ownerを保持し、状態そのものを送信へ保持する契約には変更しない。

with_constraintで既存の物理Useを展開する際は、状態pointerに加えて状態のcontrol blockと物理ownerも一致する場合に限る。同じ状態でも別ownerを伴えば、独立した制約として保持する。既に別のnested制約を持ち二層を超える入力は拒否し、所有者を黙って捨てない。

```cpp
auto owned = arena.owner_ref();
auto physical = arena.buffer({1}).resource_use();
auto logical_state = std::make_shared<vsl::ResourceUseState>();
vsl::ResourceUse constraint{logical_state, owned};

// 元の型が必要な既存APIへ渡す明示的な境界。
std::shared_ptr<void> legacy = owned.share();
```

raw shared_ptrからの構築は移行用のopaque入口で、出所を推測しない。`share()`を通した値を再格納しても一般にはnodeは復元できない。通常のVSL factoryが作る物理実体では、正規ownerとImplのnodeのcontrol blockが一致した場合にだけ復元する。低水準で手作りした別ownerの内部を追跡する保証はない。

## 固定依存と可変依存

ResourceArenaは既定で従来のFIXED方針を維持する。構築時の依存以外へのbinding更新は拒否する。DYNAMICを明示すると、bindingごとの外部ownerを保持し、更新時に既知の管理root間の循環を拒否する。

```cpp
namespace l = vsl::lifetime;
l::ResourceArena arena(device, plan, inputs, l::DependencyPolicy::DYNAMIC);
auto set = vsl::typed::TypedDescriptorSet<Set>::FromResource(
    device, arena.descriptor_set(set_id), vsl::ResourceRetention::COMPLETION);
set.write<0>(UniformView::FromView(other.buffer(buffer_id), 1));
```

方針はrootに固定し、FromResourceで作った別wrapperから変更できない。同rootと構築時の固定依存は内部借用へ正規化する。可変外部ownerは各binding要素のmetadataで保持し、最後の参照が外れた後に解放する。固定依存はroot破棄まで保持する。同じcontrol blockを重複排除するとき、opaqueと既知nodeが混在したら既知情報を維持する。

heap Setも同じ更新処理を使う。Pool／Layoutの固定親を2要素の配列で保持し、親のownerもgraphに含める。例えばarena AのPoolからheap Set Hを生成した場合、H→Aはbindingが空でも存在する。AのSetへHを論理ownerとして登録するA→Hは拒否する。

## 更新と破棄

graphの辺は固定依存とSetの実際の所有metadataから列挙する。別のgraph表やnode用control blockを確保しない。新しいS→TはTからSへの到達性をiterative DFSで検査する。今回変更する辺は同じSから出るため、旧graphで検査できる。

更新はmetadataと所有の準備、root mutation guard、idleの再検査、cycle検査、native更新、metadataのswap、guard解除、旧ownerの解放の順とする。writeと移行用ResourceUseGroup::replaceは同じ契約を使う。部分更新では未更新要素の所有を残す。native callbackから更新中rootへ逆辺を追加することも拒否する。旧ownerのcallbackは公開済みgraphを参照でき、更新を再入できる。

Poolの明示破棄では、必要なsnapshotを先に確保する。全Setのhandleとbinding mapを無効化・swapしてから旧ownerを解放する。コンテナ破棄中のcallbackが、途中のmapをgraphとして走査することを防ぐ。

rootの最終破棄はnodeをretiringにしてから、Set等のnative／CPU実体、外部依存の順で行う。GPU記録と完了までの保持は既存のCompletion／workを用いる。通常の記録は保存済みImpl*とstate*を使い、graph探索を行わない。保持境界は既存shared_ptrをprivateに借用して渡し、OwnedRefから一時的なshared_ptrコピーを増やさない。

## custom rootの必須条件

公開DependencyNodeはAHOや利用側の所有境界へ埋め込むための下位APIである。control block一致の検査だけでは、以下の条件は証明できない。

- nodeを所有者内に埋め込み、参照するOwnedRefがある間はnodeも生存させる。公開後は移動しない。
- 一つのcontrol blockに正規nodeを一つだけ設ける。
- visitorは固定依存と可変依存の実際の強所有を漏れなく、副作用なしに列挙する。
- root destructorの先頭でretireし、破棄途中のmetadataを列挙させない。
- 関連root間の更新・破棄を呼出側で直列化する。descriptor単体のVulkan host synchronizationだけでは、同時のA→B／B→A検査を防げない。

stack上の別nodeへ任意のshared_ptrをpublishする使い方は、この寿命条件を満たさない。Opaque owner内部の任意の所有、visitorから省かれた所有、低水準aliasの不正な寿命を自動で安全にするAPIではない。

## 互換性と残る接続

通常VSLの生成・更新とTyped APIの呼び方を維持する。ResourceUseとBufferViewのABIは変わるため、利用側を再ビルドする。weak_ptr等の既存APIが必要な場合は`use.state.share()`のように明示する。BufferView::owner()とlegacy_owner()は移行用のraw shared入口、owner_ref()は出所を保つ入口として分ける。

一括領域は引き続きCPU Implと物理状態を対象とする。各Implのnode pointer、外側rootのnode、OwnedRefの出所情報の分だけmetadataは増える。node自体の個別確保はなく、DFS scratch、bindingコンテナ、動的ownerのbinding間重複保持、Vulkan device memoryは別に残る。性能率は未測定。

AHOのBufferではphysical storage P・logical state L・custom generation R、TextureではData D→Image Pの固定所有を区別する。これらのAHO root visitorとTyped bundle内のSet／Pool／Layout実配置は、次の接続で行う。Device／起動・command／WSIの所有境界も完了計画の後続に残る。現段階で全VSL／AHOの所有をgraph化したとは扱わない。

## 検証

CPU7件とGPU6件を追加した。pair代入／reset callback、alias／move／opaque化、正規control block、knownへの昇格、null alias、両方向cycleと辺除去、同stateの別owner、更新中rootへの逆辺拒否を確認した。GPUでは部分array更新の未変更要素、固定のimport Layout／HeapSet親、旧GPU値、最後のbinding差し替え後の回収、公開後の解放callback再入を確認した。

macOS arm64／Clang20／C++26、ASan・UBSan、MoltenVK・Vulkan validationで、VSL808件中805成功・既知packed format3skip、AHO GPU140件・CPU1070件成功。compile契約175件、strict30C++／18TU・state修正3C++／3TU・保持境界3C++／2TU・最後のnull state修正4C++／4TUも違反0。CMake280パス、MSBuild／filtersの新4パスを検査した。

公開ヘッダーだけのinstalled consumer6targetがbuild／link／CPU実行し、新public header提供とprivate Impl／owner_access非公開を確認した。Debug／Releaseの実寸法assert・CPU実行、同寸法でも異artifactのlink拒否、x86_64 cross archiveが成功した。このarm64構成で6種Implを一つずつ配置する領域は584 byte・alignment 8である。

heap／arenaサンプルは3frame・12submission・2draw、238831更新画素とresize、pipeline生成1回を維持し、PNG3枚が一致した。VUID／ASan／UBSan errorはない。LSan、Windows／Linux実機、CPU並行、実DeviceLost、全native失敗位置、性能率は未検証。driver最終メモリ表示をリーク不在の証拠として扱わない。
