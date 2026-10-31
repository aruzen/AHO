# Bufferの共通backendとTyped CPU配置

2026-10-31。[再設計案](ownership-and-placement.md)の最初の移行経路を、`8959e5bf`を基点に実装した。ホスト可視Bufferの複数slot配置とTyped転送を対象とし、image／descriptor／pipelineの配置変更やGPUメモリsuballocationは行わない。

## 実装した境界

| 責務 | 実装 |
| --- | --- |
| CPUメモリ | `storage::ArenaMemory`／private `CPURegion`。aligned allocation、外部領域adopt、容量・alignment・使用権、返却 |
| Vulkan操作 | private `backend::initialize_buffer`／`destroy_buffer`／`map_buffer`／`synchronize_buffer`／`validate_copy`／`record_copy`。解決済みImpl参照と具体的なnative操作 |
| 構築・所有・破棄 | `lifetime::construct_buffer`／`destroy_buffer`、HeapBufferOwner、ArenaOwner、完了時retention解放。slot構築、prefix journal、rollback、外部ownerの保持 |
| 同期 | 既存ResourceUseState、`sync::buffer_copy_visibility`、Completionの完了通知。使用中のhost更新拒否、barrier方針、送信結果 |

通常の`BufferAccessor::MakeBuffer`もheap ownerが持つ領域へ`lifetime::construct_buffer`し、Typedと同じbackendを使う。native生成・memory type選択・map／flush／invalidate・転送の検証を新しい生成経路へ複製していない。

BufferImplはprivateに移し、deviceと使用状態を**借用ポインター**で持つ。deviceを強く保持するのはHeapBufferOwner／ArenaOwner。backendはarenaや所有者を探索せず、invokeは捕捉済みのImplポインターからnative handleを使う。

map／同期／native破棄はImplに保存した`vk::Device`を直接使う。通常Bufferにも使用状態とnative handleのmetadataを追加するため、単体のCPU占有量が減るとは主張しない。

同期方針とnative操作は、privateな値だけの契約`BufferCopyVisibility`を共有する。sync実装はBufferImplやbackend操作をincludeせず、backendは渡されたstage／accessを適用する。

saitekikuukanの`buffer`がStructure／Layout／Identityを分ける方針、固定storageと動的storageで操作を共通化する方針、rvalueから借用を取得させない方針を参考にした。VSLのImplは利用者側では不完全型であり、GPU寿命も持つため、saitekikuukanのcontainerをそのままImpl所有者にせず、slot schema・CPU memory・native操作を分けて接続している。VSLへ新たなsaitekikuukan依存は追加していない。

参考コードはsaitekikuukanの`0e608984`にある`storage::basic_columns`／`soa_buffer`と、rvalueのcolumn取得を禁止するoverload。

## 通常のVSL利用

以下では初期化済みの`device`と`manager`を受け取る。

```cpp
#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/commands/copy_buffer.hpp>
#include <VSL/vulkan/phase.hpp>

constexpr auto usage = vsl::MemoryType::TRANSFER_SOURCE |
                       vsl::MemoryType::TRANSFER_DESTINATION;
using Buffer = vsl::Buffer<usage, vsl::MemoryProperty::HOST_VISIBLE>;
const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
Buffer source(device, sizeof(values)), destination(device, sizeof(values));
source.copy(values.data(), values.size());
auto phase = manager.start_phase<vsl::ComputePhase>();
phase << vsl::command::CopyBuffer(&source, &destination, sizeof(values), 0, 0, true);
phase.submit();
phase.completion().wait();
auto mapped = destination.data();
auto result = mapped.read(sizeof(values));
```

constructor・mapping・共有コピー・既存コマンドの呼び方は維持する。旧CopyBuffer overloadの保持期間も従来のままとし、古いCompletionを保持するとその経路の資源も残る。

## Typedの一括配置

```cpp
#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/vulkan/phase.hpp>

namespace t = vsl::typed;
constexpr auto usage = vsl::MemoryType::TRANSFER_SOURCE |
                       vsl::MemoryType::TRANSFER_DESTINATION;
using Arena = t::TypedBufferArena<
    t::BufferSlot<2, 0, std::uint32_t, usage>,
    t::BufferSlot<2, 0, std::uint32_t, usage, 1>>;
constexpr Arena::Plan plan({4, 4});
Arena arena(device, plan);
auto source = arena.view<2, 0>();
auto destination = arena.view<2, 0, 1>();
source.copy(std::array<std::uint32_t, 4>{1, 2, 3, 4});
auto command = source.copy_to(destination);
auto phase = manager.start_phase<vsl::ComputePhase>();
phase << command;
phase.submit();
phase.completion().wait();
auto bytes = destination.read_bytes();
```

slot keyはset／binding／descriptor配列要素／frame番号。Impl型は同じでも各slotのoffset・実体・使用状態は独立する。payload個数はplanへ明示し、コンパイル時定数にも実行時の値にもできる。既存ResourceAllocationSchemaからframe／scopeを自動展開する機能は今回追加していない。

`PlacedBufferView`は実体とoffset／rangeを固定し、arena全体を保持する。部分viewも同じownerを保持する。copyのrecord数・usage・payload型を検査する。`copy_to`はTypedな引数検査を終えたCopyBufferを作り、通常BufferとはBufferView adapterで混在できる。

```cpp
using View = t::PlacedBufferView<std::uint32_t, usage>;
auto output = View::FromBuffer(existing_vsl_buffer);
auto command = arena.view<2, 0>().view(1, 2).copy_to(output.view(0, 2));
```

借用は`arena.storage().borrow(index)`から明示的な`BufferRef`として取得する。これはarenaの有効scope内だけで使い、arenaを保持しない。rvalueからのborrowは禁止する。安全に外部保存する場合はowning viewを使う。viewから返るbyte spanも、対応view／ownerの有効期間内で利用する。

## CPU領域と構築

`BufferLayout`はImpl・ResourceUseStateの寸法からslot strideを計算する。各slotはBufferImplとResourceUseStateのC++本体を同じCPU領域に持つ。Typed planはslot数・offset・全体bytes・alignmentを計算する。

既定ではaligned領域を確保する。再利用や外部allocatorとの接続にはCPU memoryを明示する。

```cpp
vsl::storage::ArenaMemory memory(Arena::Plan::BYTES, Arena::Plan::ALIGNMENT);
{
    Arena first(device, plan, memory);
    // firstのview・command・送信が残る間は、同じmemoryへの再構築を拒否。
}
Arena next(device, plan, memory); // 保持が全て終了していれば再利用可能
```

外部領域は`ArenaMemory(span<byte>, alignment, owner)`でadoptできる。ownerは領域を実際に保持し、同じ領域を別のArenaMemoryとして重複adoptしないことが利用側の契約。同じArenaMemoryのコピー間ではclaimを共有し、二重構築を拒否する。CPU側の並行構築・操作は利用側で直列化する。

owner tokenはcontrol blockの有無で判定する。格納ポインターがnullでもcontrol blockを共有するtokenは保持し、空のshared_ptrから非nullポインターだけをaliasした値は拒否する。任意のownerが本当に対象領域を所有するかまでは自動判定できず、利用側の契約となる。

構築はprefix順、破棄は逆順。native初期化が途中で失敗するとbackendが部分生成したhandleを回収し、lifetime側がC++実体を破棄する。ArenaOwnerは構築済みprefixを巻き戻してmemoryのclaimを返す。journal用vectorは作らず、構築済み個数を使う。

全slotの成功前にarenaは公開されないため、未構築slotの公開参照は作れない。範囲外borrow、容量不足、alignment不一致、個数・byte計算のoverflow、0レコード、重複keyも拒否する。空planは0 slot・0 byte・alignment 1とする。

CPU領域をreallocしてliveなImplを移動する操作はない。旧借用を保存したまま領域を再利用することは禁止し、owning viewがある場合はclaimによって再利用を防ぐ。

## 記録・完了・破棄の実経路

```text
Typed plan -> ArenaMemory -> ArenaOwner
  -> 各offsetにBufferImpl / ResourceUseStateを構築
  -> 共通backendがnative bufferと専用device memoryを生成

owning view -> CopyBuffer
  -> Implポインターとownerを別々に捕捉
  -> record前にdevice/range/usageと使用状態を検証
  -> submissionのcompletion_resourcesへownerを保持（同じarenaは重複排除）
  -> native handleを直接使ってcopyとbarrierを記録
  -> managerのframe slotが送信レコードを保持

Completion.status / waitがGPU完了を確認
  -> 非所有の完了フックを通知
  -> lifetime側がcompletion_resourcesを解放
  -> 最後のview/command/送信pinが外れる
  -> native破棄 -> C++実体破棄 -> CPU memoryのclaim返却
  -> memoryの最後のownerでCPU領域を返却
```

同期コードは寿命管理の具体的な実装をincludeしない。完了フックは関数ポインターで、arenaを捕捉しない。保持資源の解放は寿命管理側の処理。これは既存SubmissionImplを使う移行用接続であり、最終的なCompletionSignal／InFlightRecord分離が全体へ完了したわけではない。

cancelは未送信のnative記録をresetしてから新経路のpinを解放する。元のCompletionはCANCELLEDとして観測できる。例外巻き戻しによるPhase取消でも回収する。送信済みのcancelでGPU処理を取り消したことにはしない。

新経路のpinは完了確認／cancel時に解放するが、commandやview自身が存続すれば資源も存続する。完了の能動的な監視threadは追加せず、完了poll／wait、slot再利用、manager破棄の既存経路で回収する。

## 生成レイアウトとインストール

`VSLi/cmake/BufferLayout.cmake`がprivate実定義をtarget compilerでcompile-only検査する。size／alignment・standard library ABI modeを取得し、configごとにfingerprint付きヘッダーを生成する。target executableの実行やGPUは不要。

- libraryのbuffer_layout.cppが実sizeof／alignof／slot offset／strideと一致をstatic_assertする。
- consumerには数値定数だけを公開し、Implの完全定義はinstallしない。既存の`_pimpls.hpp`もsrc/privateへ移した。
- pointer ABI、compiler系統・major、標準library ABI mode、公開UseStateの寸法を確認する。
- consumerのarena構築は生成ABI IDを含むsymbolを参照する。異なるheader／binaryの組合せをlink時に拒否する。
- CMakeのVSL::VSL targetが生成headerの選択とincludeを伝播し、installにも同じheaderと定義を提供する。
- probe結果のcache keyにはprivate定義・SDK・toolchain・config flags等を含め、header変更をconfigure再実行依存へ登録する。

multi-configでは使用するconfigを明示する。別configへの暗黙import mappingによるABIの推測は行わず、不一致なら拒否する。単一archを対象とし、universal binaryは未対応としてconfigure時に止める。CMake以外の直接MSBuild経路は通常Bufferのsource登録を維持するが、新しい配置APIにはCMakeによる生成情報が必要。

## 残っている確保・参照・adapter

- 既定arenaはCPU領域一つと、CPURegion／ArenaOwnerの共有control blockを持つ。Implごとのshared_ptr control blockは作らないが、CPU確保が全体で一回になったわけではない。
- ResourceUseStateのweak submission vector、submissionのowner vector等の要素用確保は残る。GPUの専用device memory確保もBufferごとに行う。
- 通常Bufferはheap ownerのinline領域を使う。既存TypedUniformBuffer等が別に作る使用状態、descriptor／frame／pipelineの従来の個別確保は維持する。
- `.data_`は移行用のaliasing shared_ptrとして残す。BufferViewのlegacy_owner／resource_useも既存Typed bindingへの境界adapterであり、arena内部へ格納しない。
- 既存`get_device()`は所有するAccessorを返すため、device側のshared_from_thisを使うadapterを残す。BufferImpl自身はdeviceを所有しない。将来deviceもarena化する前に、この境界を再設計する必要がある。
- 通常操作はImplへ直接届くが、legacy facade、virtual command entry、manager、deviceへの参照を全廃したわけではない。同期付きの旧copy_by_buffer等も既存経路を残す。
- 旧Completionのresources保持は変更せず、新経路の保持だけをterminalフックで分離した。readbackやpresentation全体は未移行。

## 次の範囲と制約

次は既存ResourceAllocationSchemaからこのCPU planを導出し、SHARED／PER_FRAME／配列を自動展開すること。その前提として、arena内部descriptorにはowning ResourceUseを格納せず、borrowと外部domain依存を分ける設計を実装する必要がある。

AHOの共有Buffer再確保・旧view snapshot・Custom所有者は今回は変更しない。AHO native()経由のescapeを新しい同期保証へ自動接続するものでもない。型付き転送APIで作ったviewの使用状態と、既存AHO側の状態を統合する段階が必要。

新しいbusy検査はBufferView／PlacedBufferView経由の操作に適用する。旧Buffer::data／copyやnative escapeがこの状態を自動追跡するようには変更しておらず、それらを混在させる場合の同期は従来通り利用側の責任。

異なるqueue間の所有権移動、並行CPU操作、device lost等で結果が不確定なsubmit失敗の回収方針、image／descriptor／pipeline／presentation全体の配置は未解決または未移行。今回の正常完了・未送信cancelの検証と区別する。

速度向上率は測定していない。構造上は、複数Implと使用状態の本体を一領域へ配置し、Implごとの強いdevice所有とcontrol blockをなくし、記録時の実体参照を事前解決し、送信側のarena pinを重複排除している。

## 検証記録

macOS arm64／Apple M1、Clang 20.1.8、C++26、CMake 4.1.1、Release `-O1 -g0`、ASan／UBSan、Vulkan SDK 1.4.309.0、MoltenVK 1.4.2で検証した。

| 検証 | 結果 |
| --- | --- |
| VSL全体 | 685成功、3 skip。新規9件を含む |
| AHO GPU | 119成功。旧Buffer／Typed連携・再確保・共有所有の回帰を含む |
| AHO CPU | 1070成功 |
| Typed compile契約 | 122成功。新規7件はpayload／usage／slot／一時arenaからのborrow／重複key等を検証 |
| 既存Typedサンプル | 120フレーム描画、verifyで3 frame slot／12 submission／2 draw／resize／pipeline build 1回を確認 |
| インストール | prefixを移動したconsumerがpublic headerだけでbuild／link／CPU初期化検証に成功。private Impl headerは非配布 |
| ABI不整合 | 別Debug構成の生成headerとRelease binaryを混ぜたconsumerは、ABI固有symbol未定義で意図通りlink失敗 |
| multi-config | Ninja Multi-ConfigのRelease／Debugで別headerを生成し、各configのbuffer_layout.cppをsyntax-only compileして実寸法assert成功 |
| cross target | x86_64指定のconfigure成功。probe archiveがx86_64のobjectであることを確認。target executableは実行していない |
| 規約 | 変更したC++のstrict検査で違反0。最終の所有判定・契約header分離後も対象TUを再検査 |

この構成ではBufferImplは80 bytes／alignment 8、ResourceUseStateは24 bytes／alignment 8、slot strideは104 bytes。これはこのartifactの値で、公開ABIの固定値ではない。

GPUテストは必要なvalidationを有効にし、新規fixtureでは破棄までのvalidation errorも失敗とする。macOSではLeakSanitizerを使わず、ASan／UBSan、validation、weak owner失効、外部領域の返却回数、失敗後のregion再利用で寿命を検証する。GoogleTestも同じClang／sanitizerの静的libraryを使い、通常Homebrew buildとのcontainer annotation混在を避けた。

旧master由来のStandardDrawerの不足includeと、GPUテストの旧Engine所有者アクセスは、今回の関連targetをbuildするため公開RenderContext経由へ修正した。一方、旧`AHO/Main.cpp`のWindowDataを7要素でstructured bindingするコードは、現在の8要素と合わずbuildできないことを確認した。今回のTypedサンプルとは別の古いアプリケーションで、ここは変更していない。

Windows／Linuxでのbuild・実GPU、universal binary、x86_64 binaryの実行、device lost／不確定なsubmit失敗時の回収は未検証。CPU単体のメタデータ量、確保回数、時間の比較測定は未実施。
