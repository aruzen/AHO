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

`OwnedRef::weak()`は出所を保つ`WeakOwnedRef<T>`を返す。lockは先にstrong所有を取得し、use_countが正のときだけ同じnodeを伴うOwnedRefを返す。null aliasにも対応し、期限切れではnodeへ触れない。weakのmove／代入／resetもpointerとnodeを一体で扱う。raw weak入力は明示的な`WeakOwnedRef::Opaque()`に限り、nodeを推測しない。

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

AHOの接続は次節の範囲まで行い、[Typed bundleのSet／Pool／Layout実配置](typed-descriptor-placement.md)も同じgraphへ接続した。Device／起動・command／WSIの所有境界は完了計画の後続に残る。現段階で全VSL／AHOの所有をgraph化したとは扱わない。

## AHOの世代と固定親

Bufferはphysical storage P、独立したlogical state L、CUSTOM／通常Bufferのgeneration Rを分ける。Rにnodeを埋め込み、Rが所有するStorageのPを固定辺として列挙する。Lは資源を所有しないleafで、旧snapshotと新世代の双方から同じ論理制約を利用する。Rのdestructorはmapping／storage／customizationの破棄より先にnodeをretireする。

BufferStateのweak resourceは出所を保持する。`BufferBinding::owner_ref()`とsnapshotはRの出所を維持し、互換用`owner()`だけがraw shared_ptrへ戻す。ArenaBuffer／ImportedBufferでは返却viewのPを直接使い、世代用ownerで包み直さない。同じarenaの別slotを複数のAHO Bufferへ渡してもcontrol blockは共通のままである。

再確保はcandidateの生成・mapping・copy／writeを成功させてから、native／strong owner／weak owner／view／最終sizeを切り替える。旧Resource、旧view、native互換adapterは局所へ退避し、commit後に解放する。旧ownerの解放callbackは新しいsnapshot／size／内容を参照でき、更新APIも再入できる。prepareと完了pollの途中の同じ論理Bufferへの再入変更は`BufferMutation`で拒否する。局所state所有はcallbackが元facadeを破棄した場合にもcommitと解放を完了するための保持であり、送信へBufferStateを保持する変更ではない。

TextureはData DのnodeからImage Pへの固定辺を列挙する。Dは物理状態と異なる論理状態を持ち、resource_useにはDのchecked state／ownerを制約として渡す。完了結果の観測とD／Pの保持は引き続き別の契約である。

```cpp
auto buffer = aho::resource::make_arena_buffer(device, 16);
auto old = buffer.binding().snapshot();
auto weak = buffer.binding().owner_ref().weak();
buffer.resize(64);
// oldは旧Pと共通Lを保持する。新世代やfactory設定は保持しない。
auto old_owner = weak.lock();
```

Customization／custom mappingの任意の内部所有、factory closure、公開native／data_による直接置換は今回のgraph保証の外側にある。これらは呼出側が非循環と寿命を保証する移行境界であり、nodeを付けたことで内部の任意所有まで追跡できるとは扱わない。関連rootのhost操作は外部で直列化する。custom mapping callbackからGPUの競合使用を追加することは許可しない。raw spanを返す既存read()の長期アクセス契約も変更せず、安全な呼出内コピーにはread_into()を使用する。

今回追加したのは既存R／D内のnodeとweak参照の出所で、arena世代wrapperやnode用の個別確保は増やさない。R、logical leaf、既存adapter、mapping／コンテナの個別確保は移行途中として残る。初期化と再確保のcycle検査にはDFS scratchを使う。性能率は未測定である。

## 検証

CPU7件とGPU6件を追加した。pair代入／reset callback、alias／move／opaque化、正規control block、knownへの昇格、null alias、両方向cycleと辺除去、同stateの別owner、更新中rootへの逆辺拒否を確認した。GPUでは部分array更新の未変更要素、固定のimport Layout／HeapSet親、旧GPU値、最後のbinding差し替え後の回収、公開後の解放callback再入を確認した。

macOS arm64／Clang20／C++26、ASan・UBSan、MoltenVK・Vulkan validationで、VSL808件中805成功・既知packed format3skip、AHO GPU140件・CPU1070件成功。compile契約175件、strict30C++／18TU・state修正3C++／3TU・保持境界3C++／2TU・最後のnull state修正4C++／4TUも違反0。CMake280パス、MSBuild／filtersの新4パスを検査した。

公開ヘッダーだけのinstalled consumer6targetがbuild／link／CPU実行し、新public header提供とprivate Impl／owner_access非公開を確認した。Debug／Releaseの実寸法assert・CPU実行、同寸法でも異artifactのlink拒否、x86_64 cross archiveが成功した。このarm64構成で6種Implを一つずつ配置する領域は584 byte・alignment 8である。

heap／arenaサンプルは3frame・12submission・2draw、238831更新画素とresize、pipeline生成1回を維持し、PNG3枚が一致した。VUID／ASan／UBSan errorはない。LSan、Windows／Linux実機、CPU並行、実DeviceLost、全native失敗位置、性能率は未検証。driver最終メモリ表示をリーク不在の証拠として扱わない。

AHO接続ではCPU2件・GPU9件を追加した。weakの期限切れ／null alias／型変換、R→Pを通る間接cycle、DとPの個別回収、候補書込み失敗、prepare中の再入変更拒否、旧owner callbackが観測する新世代の内容を確認した。prepare callbackがfacadeを破棄する3操作、完了callbackがBuffer／Binding／Snapshotを破棄する5入口は、heap／arena双方でASan・UBSan下を通した。既存の同arena重複排除と旧snapshot寿命も維持する。

最終コードでVSL810件中807成功・既知3skip、AHO GPU149件／CPU1070件、compile契約175件、strict13C++／11TU違反0、CMake280パス検査が成功した。installed consumerはAHO／WeakOwnedRefを加えた7targetのbuild／link／CPU実行、private定義非公開を確認した。Debug／Releaseで実寸法288／584 byte（alignment 8）、x86_64 cross archiveと異artifactのlink拒否を再確認した。両描画サンプルの実測結果とPNG3枚は前記と同じである。独立レビュー2名の指摘を反映し、VUID／sanitizer errorはない。上記の未検証環境・性能率は今回も未測定のままとする。
