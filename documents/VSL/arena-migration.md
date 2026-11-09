# 4責務とTyped arenaの移行

2026-11-08追記: 以後の最終範囲と実装順序は[完了計画](arena-completion-plan.md)を参照。Instance／Deviceの配置、Typedのarena既定化、旧公開入口の廃止まで対象とする。以下は既存の実装経緯を含む。

## 順序11aの生成既定と移行

`storage/typed_storage_policy.hpp`の`DefaultStorage`は`ArenaStorage<>`を表す。TypedResourceBundle／TypedResourceFrame／TypedSharedResourcesの省略時policyと、TypedFrameCommandsの2引数生成はこの既定を使う。単体TypedBufferも以下の11b経路で既定化する。TypedPipelineの生成policyは後続11bで扱い、全Typed生成の移行完了とはしない。

```cpp
using Bundle = t::TypedResourceBundle<Allocation>; // arena
using HeapBundle = t::TypedResourceBundle<Allocation, t::HeapStorage>;
t::TypedFrameCommands commands(device, 3); // arena
t::TypedFrameCommands heap_commands(device, 3, t::HeapStorage{});
```

保持の既定COMPLETIONと固定依存FIXEDは配置と独立して維持する。heap／arena比較には同じ保持契約を使い、`aho-typed-example`は明示HeapStorage、arena版は明示ArenaStorageを選ぶ。

既定Bundleの`buffer()`はPlacedResourceBufferViewを返し、`backend()`はBufferViewとなる。旧BufferAccessorの`.data_`や直接mappingへ依存する場合は明示HeapStorageで移行を分けられる。通常の読み取りは`buffer.read_into(destination)`へ、所有境界の観測は`buffer.backend().owner_ref()`へ移す。callbackの具体型は`Bundle::Frame`／`Bundle::Shared`を使うと、選択したpolicyと型を揃えられる。

arena生成は渡されたdescriptor Managerの`allocate()` overrideを使わない。元Managerの所有とDeviceの検査は維持するが、独自allocatorやその失敗hookが必要な利用は明示HeapStorageを選ぶ。既定templateの具体型が変わるため、その型を公開する利用側ライブラリも含めて再buildする。

```cpp
std::array<std::uint32_t, 1> result{};
// GPU完了を観測した後に結果をCPU側の値へコピーする。
frame.buffer<0, 1>().read_into(result);
```

CPU arenaの配置単位とSHARED／PER_FRAMEの寿命群は変えず、Vulkan device memoryや内部vectorの確保も統合しない。公開APIと内部確保の残りは11b／12で扱い、性能向上率は未測定である。

## 順序11b前半: 単体Typed Buffer

TypedVertexBuffer／TypedIndexBuffer／TypedResourceBufferのコンストラクタは、末尾の配置policyを省略するとDefaultStorageを使う。HeapStorageと空のArenaStorage<>を選べる。binding map付きArenaStorageは単体生成へ適用しない。単体arenaは一つのBuffer slotを持ち、複数資源の集約にはTypedBufferArena／TypedResourceBundleを使う。

```cpp
t::TypedVertexBuffer<Vertex> vertices(device, vertex_span); // arena
t::TypedIndexBuffer<std::uint16_t> indices(device, index_span, t::HeapStorage{});
t::TypedStorageBuffer<std::uint32_t> values(device, std::in_place, 4);
std::array<std::uint32_t, 4> result{};
values.read_into(result);
```

heapはmake_heap_buffer、arenaはBufferArenaから、同じconstruct_buffer／operation::initialize_bufferへ進む。永続値はどちらもowning BufferViewで、物理storageの使用状態を共有する。BufferArenaのCPU配置はメモリ属性から独立させ、device-localも同じbackendで構築する。Vulkanのメモリ選択・確保とCPUのImpl配置は別である。

device-local生成には従来どおり同Deviceの所有するCommandManagerを渡す。独立した一slot command ringへ共通CopyBufferを記録し、そのCompletionを待ってから返す。元managerの記録・slotは進めない。stagingも同じ配置policyを使う。既存のgraphics queue familyとEXCLUSIVEの前提を維持し、queue familyの所有移譲は追加しない。

backend()の戻り値は、HeapStorageを選んだ場合もconst BufferView&へ変わる。一時オブジェクトからの借用は拒否し、保存したい場合はstorage_view()で所有値を取得する。旧backend().data_／data()依存は移行が必要である。Native descriptorにはBufferView更新入口を追加した。

```cpp
auto selected = vertices.view(2, 3);
auto storage = selected.storage_view(); // wrapperが消えても選択済み範囲を保持
phase << vsl::command::BindVertexBufferAt(0, storage); // offsetはview内の相対値
native_descriptor.update(values.backend(), binding);
```

storage_view()には既に選択範囲の絶対offsetが入る。低位bindへbyte_offset()を重ねて渡さず、通常は相対offset 0を使う。Typed pipelineのbindはこれを処理する。読み取りはread_into(destination, first)を使い、GPU完了後に呼び出し内でCPUの値へコピーする。

IndexのCPU snapshotはNative生成・転送より先に一度作り、その同じ値を転送する。Native callback中に元spanが変更されても、bounds検査とGPU内容を一致させる。固定個数・revisionによる古いdrawの拒否は維持する。このrevision保証はTypedIndexBuffer::copy経由の更新が対象で、低位のstorage_view().write／refへの書き込みはCPU snapshotを更新しない。

構築・記録前の失敗は局所ownerで巻き戻す。既知の送信失敗は既存の取消経路で解放する。enqueue不明の転送はheadless Workのquarantineが保持し、元managerまたは同Deviceのmanager.wait_idle()による証明後に回収する。Fence観測が失敗した場合、独立Domainの終了はFence／Device idleを再試行するため、例外伝播まで待機する場合がある。logical FAILEDでも既知ENQUEUEDのnative使用が残れば、完了観測が済むまで保持を外さない。

旧commandがshared_ptr<BufferImpl>を受ける箇所へのlegacy_owner aliasは順序12で除去する移行用である。新しいcontrol blockを作らず、元の所有境界を共有する。Index snapshotのvector、arenaの管理領域、独立command ring／Fence、Impl内部コンテナ、Vulkan device memoryの個別確保は残る。単体一slot arenaがheapより少ない確保になるとは主張しない。配置と独立したCOMPLETION保持を揃えた測定は順序12で行う。

## 順序11b後半: Typed Pipeline

TypedGraphicsPipeline／TypedComputePipelineは省略時にDefaultStorageを使い、先頭のpolicy引数でheap／arenaを選択する。class templateの型引数と返すPipeline facadeは共通である。shared TypedDescriptorLayoutsを渡す既存形にも同じ先頭引数を使える。

```cpp
t::TypedGraphicsPipeline<Input, Resources, Pushes> graphics(
    device, pass, shader_group, state, viewport, scissor); // arena
t::TypedComputePipeline<Resources> compute(
    t::HeapStorage{}, device, shared_layouts, compute_shader_group);
```

lifetime::PipelineRecipeDraftは既存の編集accessorを内部で使い、CPU側でstageを注入する。snapshot()は別の不変recipeを作り、stageが保存したEditorから独立させる。Graphicsのstages→Input→Resources→Pushes、Computeのstages→Resources→Pushesの順を維持する。Vertex shader、descriptor、push、Compute shaderの種類／個数をsnapshotに対して検証し、成功後にNative Layoutを生成する。

schemaによるdescriptor layout定義はDevice limit検査の後に遅延保存する。明示ResourceBindingとschemaは最後の注入が勝ち、両者はshader／bindingのowner index再配置と重複排除を共用する。通常のPipelineLayout constructor／copy／add／FromRuntimeはinit_finishで遅延定義を構築し、従来のNative Layoutを持つ結果を維持する。

構築に必要なDescriptorLayoutは一時寿命群に置き、heap／arenaとも空set／holeのLayoutを共有する。arena planのcompile時容量はResources::COUNT+1であり、最大set番号SET_COUNTを固定領域の容量に使わない。確保するCPU領域は実際のslot数から求める。shared TypedDescriptorLayoutsを渡す場合は既存SetLayoutを構築中だけ保持する。

heapは共通make_heap_pipeline_layout／make_heap_pipeline、arenaは既存ResourceArenaのLayout＋Pipelineの2 slotを使う。runtime PipelineはLayoutを保持し、生成用recipe・Shader・RenderPass・DescriptorLayoutは構築呼出しの後に保持しない。Native構築関数はsnapshotを値で保持し、callbackで呼出元が破棄されても入力を生存させる。arenaの構築失敗は既存の逆順rollbackに従う。

記録後のPipeline保持契約は配置policyから独立し、従来のWork所有による保持を維持する。保存commandはPipelineと必要なBufferを所有する。CPU recipeのvector／snapshot、生成scratch、ResourceArena管理表、Vulkan PipelineCache等の個別確保は残る。CPU Implの2slot集約による構造変更と実測性能は区別し、測定は順序12で行う。

## 順序12a: 物理実体の比較

BufferIdentity／DescriptorIdentity／PipelineIdentityは、解決済み実体を比較する非所有の値型である。BufferRef／BufferView／通常Buffer、頂点・indexのbind command、PipelineAccessor、GraphicResourceのidentity()から取得できる。種類の違うidentity同士の比較、raw pointerからの構築／取り出し、Native handleへの変換は提供しない。

```cpp
auto whole = vertices.storage_view();
auto first = whole.view(0, sizeof(Vertex));
assert(whole.identity() == first.identity()); // 同じ物理storage、範囲は別
```

同じarena ownerの別slotは異なるidentityを持ち、同じ実体を指すaliasは同じidentityになる。比較はownerを保持せず、両方の実体が生存する間だけ行う。解放後のarena領域再利用をまたぐ世代ID／永続cache keyには使わない。schema ResourceId、配置slot、shader BindingKeyとも別の役割である。

TypedBatchはcommand群のownerを保持してplanを作るため、このscopeを満たす。Pipeline／Buffer／Descriptorの比較にはidentity、viewのoffsetとindex型には従来の値を使い、bind省略の意味を維持する。DeviceとCompletionの比較は既存operator==へ移す。生成・記録時に新しいowner wrapperや管理表の探索を追加しない。raw source()／data_／legacy_ownerは後続の公開入口廃止へ接続する移行用として、この段階では残る。

## 順序12b: descriptor入力値

typed::detail::DescriptorValueはprivate fieldを持つ値型となり、ImplのOwnedRefを直接渡すaggregate構築を閉じる。通常のTypedBuffer／TypedImageResource／bundleのdescriptor()と書込みAPIは維持する。独自のdescriptor入力を作る場合は、FromBuffer(BufferView,ResourceUse)／FromImage(ImageAccessor,ResourceUse)／FromSampler(SamplerAccessor,ResourceUse)／FromTexture(ImageAccessor,SamplerAccessor,ResourceUse)へ移す。

```cpp
using Value = t::detail::DescriptorValue;
auto value = Value::FromBuffer(buffer.storage_view());
auto selected = value.buffer_subview(relative_offset, bytes);
auto texture = Value::FromTexture(image, sampler, logical_constraint);
auto retained_image = texture.image(); // 元のCBを共有するowning accessor
```

byte_offset()は絶対byte offset、size()は選択済みのbyte数で、buffer_subviewは現在の範囲内の相対指定である。Typed viewのcountは従来どおりPayload要素数。二段Subviewでもoffsetを一度ずつ加算する。default empty／copy／moveを維持し、空値の書込み検証や配列全体の失敗保証も変えない。

resource_use()は保存された入力useの値を返す。Bufferではfactoryがphysical＋logicalを合成し、Image／Sampler／Textureでは渡されたuseを保存する。後者のphysical stateとownerはdescriptor書込み時に双方を登録するため、この値だけを記録してImage／Samplerの完全な使用登録とみなさない。通常はTypedDescriptorSetへの書込みとbindを使う。

Image／Samplerの元CB、外部composite owner、strong null aliasの意味を保持し、canonical nodeからownerを取り直さない。画像／sampler getterも同じCBを共有する。Bufferのrefへのアクセスはfactoryのcpp内へ下げ、次のBufferRef／ResourceUse公開入口廃止へ接続する。値の生成に新しい個別確保やowner wrapperは追加しない。旧aggregateへ依存する利用側はfactoryとmetadata/getterへ移して再buildする。

## 順序12c: 使用登録の公開入力

ResourceUseのfactoryはImplのshared_ptrを受けず、通常のBufferAccessor／ImageAccessor／SamplerAccessorとBufferViewを受ける。Typed descriptorのResourceUseGroupもGraphicResourceを渡す。既存のaccessor.resource_use()とview.resource_use()は維持する。

```cpp
auto physical = vsl::ResourceUse::ForBuffer(buffer.storage_view());
auto constrained = vsl::ResourceUse::ForBuffer(buffer.storage_view(), logical_constraint);
auto image_use = vsl::ResourceUse::ForImage(image);
auto group = vsl::ResourceUseGroup::ForDescriptor(
    binding_count, descriptor, vsl::ResourceRetention::COMPLETION);
```

旧ForBuffer(buffer.data_)等はForBuffer(buffer)へ、ForDescriptor(count, descriptor.data_)はForDescriptor(count, descriptor)へ移す。tracks_descriptorもraw pointerではなくGraphicResourceを受ける。同じ物理storageの状態と、入力が保持した元のcontrol blockを共通のprivate処理へ渡す。外部composite ownerをcanonical ownerへ置き換えず、Bufferの追加logical constraintも維持する。private factoryはインストールしない。

descriptorの使用状態と所有の一致は別々に判定する。同じSetを記録済みのGroupでも、commandのGraphicResourceが異なるcontrol blockを持つ場合、その追加ownerをGroupと同じ保持契約で登録する。同じcontrol blockの二重保持と物理状態の二重記録は省く。COMPLETIONでは完了／取消後に追加ownerを回収し、SUBMISSIONではWorkの寿命まで保持する。物理一致の判定へcontrol block一致を混ぜて、保持policyを切り替えることはしない。

factory入力の変更に新しいowner wrapperや使用状態を追加せず、記録は解決済み実体を使う。BufferRef／BufferViewのraw構築、legacy_owner、他の公開data_は次の移行対象として残る。

## 順序12d: Bufferの参照境界と転送入力

BufferRefは非所有の操作用参照で、raw pointerの取得・指定とaggregate構築を閉じる。BufferViewは所有値で、元のowner／nodeと選択範囲を保持する。raw ref＋owner＋rangeからの公開構築とlegacy_ownerは廃止し、通常BufferからはFromBuffer、arenaからはview／bufferを使う。内部のraw接続は非installのBufferAccessだけへ置く。

```cpp
auto storage = vsl::BufferView::FromBuffer(native_buffer);
auto selected = storage.view(4, 16); // 選択範囲を所有する
auto borrowed = selected.ref();     // selected等の所有者が生存する間だけ有効
phase << vsl::command::ImageUpload(selected, image, vsl::ImageLayout::UNDEFINED,
                                  {.width = 1, .height = 1}, 8);
```

ref()はlvalueだけから取得できる。PlacedBufferView／PlacedResourceBufferViewのbackend()も同じ借用制限へ揃えた。一時viewから保存する場合はstorage_view()の所有値を使い、その値を変数へ置いてからref()を得る。頂点／index bindのraw source getterとshared_ptr入力も廃止し、Accessor／BufferView入力とidentity()を使う。bind内部は解決済みImpl*と一つのResourceUseで所有・物理状態・論理制約を保持する。

CopyBufferToImage／ImageUploadはBufferView、追加のResourceUse constraint、source専用のResourceRetentionを受ける。offsetはview内の相対値で、Native offsetへview先頭を一度だけ加算する。view容量内を減算で検証してから、合計offsetのalignmentを確認する。旧BufferAccessor*入力は既存SUBMISSION、新view入力は既定COMPLETIONで、明示policyも選べる。画像とbarrierの既存SUBMISSION保持は維持するため、source policyを転送全体の保持期間とは扱わない。

共通prepareはsourceのphysical＋logical useを最初の画像barrierより前に登録し、同じCompletion／slot／記録contextが有効か再確認する。Native copyは登録済みの解決済み実体を使う。単独copy、ImageUpload、array copyも同じ処理へ進む。command全体を局所保存するため、過去の使用完了callbackが元commandを破棄しても、保持した入力で継続できる。取消／別記録への切替はNative記録前に拒否する。

AHOのBufferBinding.native()は廃止する。TextureUpdateはinvoke時に一度取得したBufferSnapshotからstorage／初期化済みsize／constraints／retentionを使い、使用可能性の観測中にlive Bufferが再確保されても同じsnapshotを使う。更新中の拒否は維持し、初期化済みsizeを容量とは別に検証する。NativeBufferAdapterの個別確保と世代ごとの互換参照は不要になった。旧世代は新しいsize公開とmutation終了後に回収する。

```cpp
const auto snapshot = aho_buffer.binding().snapshot();
auto storage = snapshot.storage(); // BufferViewの所有値
vsl::command::ImageUpload upload(storage, image, current_layout, region, offset,
                                 snapshot.constraints(), snapshot.retention());
```

GPUBufferBase.native()は具体的なStorage&を返す別の契約として維持する。TYPE／PROPERTYによる静的usage検査とCustom mappingは変更しない。CPU Impl配置とVulkan device memoryは引き続き別の確保であり、内部コンテナやowner境界の個別確保も残る。通常BufferAccessorのdata_を含む残りの公開入口は後続へ移行する。性能改善率は未測定である。

## 順序12e: custom commandのNative記録

opt-inのlifetime/native_recording.hppにNativeRecordingContextを追加する。記録中のCommandBufferを明示して構築し、保存したResult／slot／世代／Native CBと現在の記録先の一致を検査する。公開Implやprivateヘッダーを使わずにcustom commandのNative記録へ進める。

```cpp
const vsl::NativeRecordingContext recording(buffer); // Phaseの記録中だけ構築
recording.use(storage.resource_use());                // 物理使用と明示ownerを登録
recording.retain(external_owner);                     // 必要な外部所有境界
const VkMemoryBarrier visible{
    VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
vkCmdPipelineBarrier(recording.native_command_buffer(),
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                     0, 1, &visible, 0, nullptr, 0, nullptr);
```

use／retainの既定保持はCOMPLETIONで、SUBMISSIONも明示できる。ResourceUseの既存契約に従い、状態の追跡と明示ownerの保持を分ける。使用状態だけを渡せばその状態を一律に送信へ所有する、という契約へは変更しない。保存したcompletion()は取消・完了後の結果も観測できるが、古いcontextで再記録はできない。同じslotを再利用しても別Resultを拒否する。

use／retainはcallback後に同じ記録先を再検査し、元のcontextが破棄されても局所保存した入力で処理する。copy／move構築はでき、代入はできない。保持するownerにはcontext／Phase／Managerを含めず、Domainへ戻る所有循環を作らない。既存のCommandBuffer／Domainを保持する値型であり、新しいowner wrapper／Work所有辺／arena slotや個別確保を追加しない。

native_command_buffer()はlvalue contextから取得する借用値である。取得からvkCmd呼出しの終了まではpoll／Phase／ring操作へ再入せず、host側で直列化する。この段階ではNative呼出し全体の記録scopeは提供しない。GPUの可視性・layout・queue依存、渡すNative資源の同Device・usage・render pass scopeの妥当性はcustom command側で保証する。資源の使用登録だけでbarrierを代替しない。

capture／保持／再検査はlifetime、解決済みImplとslotからNative CBを借りる処理はoperation、recordableと使用状態は既存sync、CPU配置は既存storageを使う。C SDK型は専用headerだけから公開し、command.hpp／pv.hpp／define.hppへ新たなSDK依存を追加しない。private operationヘッダーはインストールしない。

## 順序12f: Commandの公開所有境界

CommandPool／CommandBuffer／CommandManagerの`data_`と`shared_ptr<Impl>`構築を非公開にした。空値は既定構築、存在検査は`explicit operator bool()`、同一性比較は種類別の`identity()`を使う。空値は操作可能な資源ではなく、device／pool／buffer／slot等の取得は例外で拒否する。通常構築、`FromBuffer()`、独立ring、arena配置とNativeRecordingContextは共通backendのままである。

```cpp
vsl::CommandManager empty;
vsl::CommandManager manager(device);
auto alias = manager;
auto other = vsl::CommandManager::FromBuffer(manager.get_buffer());
assert(!empty);
assert(alias.identity() == manager.identity());
assert(other.identity() != manager.identity());
assert(other.get_buffer().identity() == manager.get_buffer().identity());
```

Managerのidentityと、そのManagerが共有するring／poolのidentityは別である。identityは比較中に元の所有者を必要とする非所有値であり、保持や世代管理を代替しない。旧`CommandBuffer(nullptr)`等は既定構築へ移し、Native記録にはNativeRecordingContextを使う。

内部のCommandAccessはprivate lifetimeヘッダーだけへ配置し、解決済みImplの借用と、callbackを跨ぐ強い所有snapshotを区別する。通常のNative操作にDomainの再探索や新しい所有wrapperを追加しない。既存DomainのResolveと明示的なAttachも分離したまま、copy-and-swapと旧owner回収時の再入保証を維持する。AHOはbool／種類別identityへ移行し、VSLのprivate bridgeへ依存しない。

この単位は12fの前半である。Buffer／Image／SamplerのAccessor、起動資源、descriptor manager、Pipeline recipe、WindowDataの公開Impl入口と、実定義の4責務分割・同条件測定は引き続き残る。

最終成果物をmacOS arm64／Clang20 C++26、MoltenVK、Vulkan validation、ASan／UBSanで検証した。重点VSL209／AHO152、全VSL1,145成功（対応外format3 skip）／AHO GPU192・CPU1,071、compile契約231件が成功した。heap／arenaのPNG3枚は完全一致し、resize後のPipeline生成1回を維持する。strict82ファイル／74TUと補完検査、installed public consumer29種、新負例7種、Debug／Release／x86_64の寸法生成とartifact混在拒否、archive／生成header一致、source389件とMSBuild登録も確認した。Windows／Linux実機、device lost実機、並行CPUアクセスと性能比較は未検証である。

## 順序12f: Buffer／Image／Samplerの所有入口

3つのAccessorの公開`data_`と、Implを返す`MakeBuffer`／`MakeImage`を廃止する。通常Buffer／Imageテンプレートはprotectedの非template構築処理へInfoを渡し、Samplerも同じheap backendへ接続する。`TYPE`／`PROPERTY`／`MODE`、仮想`data()`／`flush()`とCustom Storageの意味は維持する。

runtime設定から単独heap所有で生成する場合は、Implを公開しない次のfactoryを使う。Typedの明示HeapStorageも同じ入口へ委譲し、arena既定や保持policyとは分ける。

```cpp
auto buffer = vsl::BufferView::Create(device, {
    16, vsl::MemoryType::TRANSFER_DESTINATION,
    vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT});
auto image = vsl::ImageAccessor::Create(device, {
    vsl::ImageType::TRANSFER_SRC | vsl::ImageType::TRANSFER_DST, 2, 2});
```

内部所有はAccessorごとの`OwnedRef`一つへ揃え、最初のheap境界だけでcanonical化する。descriptor更新、TypedのDescriptorValue、ResourceArenaのimport／getter、Framebuffer依存、Swapchain画像は元のnodeとcontrol blockを伝える。composite ownerの`R→A`を物理arena Aのownerへ取り直さない。同rootの内部依存は既存の正規化で借用とし、別rootには元の所有を残す。

CPU操作はpoll前に必要なowner・Impl参照・data・範囲を保存する。GetDataは同じstorageからmappingとHolderを作り、完了callbackが元Accessorを破棄／差し替えしても別世代へ移らない。BufferRefは借用のまま開始時の実体を固定し、BufferView／read_into／Holderは呼出し中の所有も保持する。viewの代入は全値を交換してから旧ownerを回収する。

仮想`data()`の後に派生実装の`flush()`を呼ぶlegacy copy／uncheck_copyでは、呼出し全体を通じてreceiverと派生状態を生存させる。自己破棄を検出して拒否する保証も追加しない。Holderの`read()`と公開dataポインタは借用であり、利用中のstorage所有とGPU競合回避は呼出側の責任である。内部MappingSnapshotは、その借用のGPUアクセス期間を管理するMappedAccessではない。CPUアクセスと記録／送信はhost側で直列化する。その条件の下で所有と同期を呼出し内で完結させる上位APIには`read_into()`を使う。

画像の読み戻しは、raw Impl入力に代わってBufferViewの範囲と追加の論理制約・保持policyを受ける。次のimageは初期化済みで、現在のlayoutがTRANSFER_DST_OPTIMALの場合である。

```cpp
const std::array<vsl::BufferCreateInfo, 2> infos{{
    {16, vsl::MemoryType::TRANSFER_DESTINATION},
    {32, vsl::MemoryType::TRANSFER_DESTINATION}}};
vsl::lifetime::BufferArena arena(device, infos);
auto destination = arena.view(1).view(8, 16);
auto phase = manager.start_phase<vsl::ComputePhase>();
phase << vsl::command::ImageReadback(
    destination, image, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
phase.submit();
phase.completion().wait();
std::array<std::byte, 16> pixels{};
destination.read_into(pixels);
```

View入力の転送元／転送先はCOMPLETIONを既定とし、SUBMISSIONは明示選択できる。Accessor*入口と画像側の旧保持契約はSUBMISSIONを維持する。資源をどこへ配置するかで保持期間を変えない。保存したview／command自体の所有は、GPU完了だけでは解除しない。

Upload／Readbackは自己snapshot、全入力の検証、使用登録と記録contextの再検査を最初のNative barrier前に済ませる。layout／rangeからbarrierを作る共通処理とNative記録はoperation、所有・使用登録との接続はlifetime側の既存処理を使う。複合操作の前後barrierでは再pollせず、取消後の別slot／peerへの誤記録を防ぐ。単独barrierも同じNative処理へ委譲する。

今回の一括配置は引き続きCPUのImplと使用状態の本体だけを対象とする。内部コンテナ、生成scratch、個別のVulkan device memoryは別の確保である。追加のowner wrapperは作らず、host snapshotとResourceUse／Native contextで残る参照カウント操作や管理メモリは、後続の同条件測定で扱う。

この単位はmacOS arm64／Clang20 C++26、MoltenVK、Vulkan validation、ASan／UBSanで検証した。新runtime11件、重点VSL132／AHO152、全VSL1,156成功（対応外format3 skip）／AHO GPU192・CPU1,071、compile契約245件が成功した。旧実装でsnapshotの2失敗を再現し、view代入の旧順序でも全値commitの回帰が失敗することを確認してから、最終実装へ復元した。heap／arenaのPNG3枚は一致し、resizeを含めPipeline生成1回を維持する。

strict79ファイル／64TUと変更箇所の補完検査、installed public consumer30種、旧入力拒否14種、private bridgeの非install、archive／生成header一致、Debug／Release／x86_64の寸法生成とartifact混在拒否、source393件とMSBuild登録も確認した。Image backendは旧SDK1.3.296でも構文検査した。Windows／Linux実機、x86_64の全体実行、device lost実機、並行CPUアクセス、性能比較は未検証である。残るShader／起動資源／descriptor manager／recipe／WindowData等の公開入口とImpl実定義の責務分割は後続の単位で扱う。

## 順序12f: Shader／ShaderGroupの所有入口

ShaderAccessorとShaderGroupの公開`data_`、ShaderAccessorのraw Impl構築を廃止する。Shaderのfile／SPIR-V配列constructor、make_shader、name／type／code／get_deviceは維持する。空判定はbool、所有境界の受渡しはowner_refを使う。code()はlvalueの借用で、参照先を使う間は所有者を生存させる。

```cpp
vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, words);
vsl::pipeline_layout::ShaderGroup group("compute", {shader});
auto alias = group;
auto moved = std::move(group);
// aliasとmovedは同じ編集内容を共有する。move元のgroupはboolで空を確認できる。
```

ShaderGroupのcopyは同じCPU編集状態を共有する。move元のadd／remove／injectionはlogic_errorを返す。removeの両入口はCPU側の一覧をコピーして編集し、swapで新しい一覧を公開してから旧ownerを回収する。ownerの解放callbackが同Groupへ追加しても、公開済みの一覧を使う。この編集scratchのvector確保と参照カウント操作は残る。

内部ではShaderAccessorとGroup内の各Shader参照に元のOwnedRefを保存する。Group→recipe→不変snapshot→生成入力の復元→ResourceArenaの外部import／getterでも、元のnodeとcontrol blockを引き継ぐ。heap factoryのshared Impl返却はprivate境界で一度だけ正規化し、以後の通常操作は保持済みImplへ直接届く。所有wrapperやcontrol blockは追加しない。

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
vsl::pipeline_layout::ShaderGroup graphics("arena", {arena.shader({1}), arena.shader({2})});
```

Shader moduleはPipeline構築呼出し中の依存であり、完成したPipelineはShader／Group／recipeを保持しない。生成用のsnapshotが残っている間は再構築用Shaderを保持する。CPU arenaに入るShaderImpl本体と、SPIR-V code／entry名の内部確保、Vulkan moduleの生成は別に扱う。OwnedRefのnode欄、Groupの共有編集状態、recipeのvector／snapshot、生成scratchの確保は後続の測定対象である。

今回の閉鎖後も、descriptor／起動資源／runtime graphics／同期資源、recipe定義とWindowDataの公開Impl入口、実定義の4責務分割は残る。

macOS arm64／Clang20 C++26、MoltenVK、Vulkan validation、ASan／UBSanで、新runtime3件と既存Typed computeのheap／arena拡張、関連130件、全VSL1,159成功（対応外format3 skip）／AHO GPU192・CPU1,071、compile契約252件を確認した。変更前のnode脱落・循環検査の見逃しと、旧in-place削除でのcallback公開順序の失敗を再現し、最終実装へbyte一致で復元している。heap／arenaのPNG3枚が一致し、resize後もPipeline生成1回を維持する。

strict17ファイル／12TU、installed public consumer30種と旧入口／一時借用7負例、private bridge非install、archive／生成header一致、Debug／Release／x86_64の寸法生成、構成混在拒否と同構成の旧親header拒否、source394件とMSBuild登録も確認した。Impl寸法定数が同じでも、公開型や内部所有型の異なるartifactは混在させない。Windows／Linux実機、x86_64全体実行、device lost実機、並行CPUアクセスと性能比較は未検証である。

## 順序12f: Descriptor／Managerの所有入口

BindingLayout／Pool／Resourceの公開`data_`とraw Impl構築を閉じ、各値がprivateなOwnedRefを一つ保存する。既存のDeviceからの生成、allocate／make／bind／update／get_poolを維持し、空判定にはbool、所有境界の受渡しにはowner_ref、Deviceの取得にはget_deviceを使う。

3型のcopy／move代入もfacade自身が値引数を受け、swapして戻り参照を評価してから旧ownerを回収する。内側のOwnedRefへの代入だけで外側の戻り順序を決めない。旧ownerの解放callbackが代入先facadeを破棄する場合、呼出し側は戻り参照を使用しない。

```cpp
vsl::graphic_resource::ExactAllocateManager manager(device);
auto [pool, resources] = manager.make(std::vector{layout});
auto resource = resources.front();
resource.update(buffer_view);
if (resource) {
    auto owner = resource.owner_ref();
    auto source = resource.get_pool();
}
```

通常bindはPoolとLayoutの元OwnedRefを固定依存へ渡す。recipe／不変snapshot／layout_sourcesの復元／arenaのexternal importとgetterでも同じnodeを保存する。Resource.get_poolはSetのcomposite ownerではなく、物理Poolのcanonical ownerへ戻る契約を維持する。external Pool importもcanonical ownerを要求する。この制約を通常bindへ広げない。

ManagerInterfaceの共有CPU stateと観測identityはprivateにする。copyは別の観測先、moveは元の観測先の移動として扱う。Exact／Overの代入は新stateとidentity、Overの設定とcacheを先に一括公開し、旧identityを無効化してから旧ownerを回収する。Base参照からの部分代入はprotected化し、Overのcacheだけが旧値になる操作を禁止する。

custom ManagerはprotectedなDevice constructorから初期化し、makeで共通生成、destroy_poolで共通の破棄検証を利用する。所有メンバーの解放callbackがget_managerを観測し得る派生型は、destructor冒頭でretire_identityを呼ぶ。独自の代入も、派生フィールドとswap_stateを公開した後に旧値を解放する。公開Implへのアクセスを代替するprotected raw getterは設けない。

builtinのmake／allocate／destroyは、入口で既存のstate・identity・必要な設定とPoolを値として保持する。pollやNative呼出し後に元のthisを読み直さない。destroy中に生成元wrapperが消えても開始時stateを操作し、同じidentityとstateを持つ生存Overだけのcacheを更新する。cache候補の確保を先に済ませ、確保失敗では所属を変更しない。custom virtual allocateの派生固有フィールドまで自己破棄に耐える保証は追加しない。

descriptor bindのinvoke／public trackは入力Resource群と必要なPipeline・設定を採取し、全Resourceの有効性を使用登録前に検査する。NativeRecordingContextで開始時の記録先を固定し、完了観測後も同じ記録か確認する。callbackがcommandを破棄・入力を変更・記録を取消・同じringで別の記録を開始しても、開始時の入力を使うか例外で停止する。取消後のpeerへ誤ってNative bindしない。

追加のowner wrapper／control blockは作らない。局所的なownerのコピー、descriptor入力vectorのsnapshot、Manager cache候補、Impl内部コンテナ、既存の共有CPU stateとidentityの個別確保は残る。Native descriptor生成・更新・破棄は既存の共通backendで行う。CPU arenaとVulkanのdevice memoryの区別、host側の直列化、独立した保持policyは維持する。

macOS arm64／Clang20 C++26、MoltenVK、Vulkan validation、ASan／UBSanで、新runtime10件（CPU1件を含む）と全VSL1,169成功（対応外format3 skip）／AHO GPU192・CPU1,071、compile契約271件を確認した。変更前には、recipeを経た元nodeの脱落とManager代入中の公開順序の不一致を実GPUで再現した。heap／arenaのPNG3枚は一致し、resize後もPipeline生成1回を維持する。

strict34ファイル／27TUと、宣言位置を修正した2ファイル／2TUの再検査が成功した。installed public consumer30種・新負例18種、private bridge非install、archive／生成header一致、Debug／Release／x86_64寸法生成、構成混在拒否と直近親の旧header拒否、source395件とMSBuild登録も確認した。確保失敗の順序はcache候補の構築を所属変更前へ置いているが、bad_allocのfault injectionはしていない。Windows／Linux実機、x86_64全体実行、device lost実機、並行CPUアクセスと性能比較は未検証である。

2026-11-06。Bufferの4責務分離が統合されたmaster `6b1f0166`を基点とする。[Bufferの契約](buffer-generation-contracts.md)と[責務の配置](buffer-layer-layout.md)を維持する。

## 方針と段階

既存APIとheap経路を併設し、arenaは明示選択する。Impl寸法生成はVSL全体で必須を継続する。通常VSLと配置経路は同じoperationを使い、内部借用と外部所有を区別する。性能向上率は未測定。

| 段階 | 実装単位と終了条件 |
| --- | --- |
| 1 | schemaからResourceId・binding map・寿命群別Buffer planを生成。型・scope・容量の矛盾を確保前に拒否 |
| 2 | bundleのarena policyとTyped viewを接続。通常descriptor／commandから使用でき、完了・取消で回収 |
| 3 | AHOの生成へ明示選択で接続。再確保後の旧view・共有alias・失敗時の旧世代維持を保証 |
| 4 | Image／Samplerの共通operation・所有・同期を分離し、異種Impl配置へ拡張。swapchain imageは外部借用 |
| 5 | descriptor layout／pool／setを配置。内部refと外部owner依存を分離し、記録時binding・更新制限・bind省略を維持 |
| 6 | shader、pipeline layout、render pass／pipeline、framebufferの順で展開。生成scratchと永続実体、互換resizeを区別 |
| 7 | 完了結果とin-flight保持を分離し、command・frame・swapchain世代へ展開。presentation側の完了をgraphics fenceと同一視しない |
| 8 | 内部vector／map・control block・保持表の確保数、保持メモリ、時間を計測し、容量指定と再利用を必要箇所へ追加 |

Device／Instanceは当面、外部の所有境界として保持する。CPU arenaにはImplと使用状態の本体を含め、内部コンテナやVulkan device memoryは別に確保する。資源の生存中にarenaを移動・拡張しない。新しい製品ディレクトリは作成前に利用者へ確認する。

## A: schemaのBuffer配置計画

`TypedBundlePlan<Allocation, BindingMap>`は既存`ResourceAllocationPlan`から次を計算する。

- bindingごとのResourceId、scope、外部資源かどうか、寿命群内のslot。
- shared群と一つのframe群それぞれのBufferCreateInfo、CPU bytes・alignment。
- shared一つと全frame群のCPU領域合計。frame数・payloadのruntime要素数は入力planからコピーする。

descriptor配列の各要素は、既定で異なる資源となる。自動IDは宣言順の空き番号で、明示IDとの衝突を避ける。IDは永続化用の番号ではない。`ResourceBinding<BindingKey, ResourceId>`の明示mapを渡すと、複数bindingを一つのowned Bufferへ接続できる。

同じIDには同じPayload型、descriptor kind、scope、要素数が必要。違う型が同じsizeofでも共有を認めない。外部資源はslotを使わず、外部資源をowned資源と同じIDへまとめる指定も拒否する。外部資源の共有は同じowning viewを複数のexternal inputへ渡して表す。

```cpp
using Map = t::BufferBindingMap<
    t::ResourceBinding<t::BindingKey{0, 0}, t::ResourceId{42}>,
    t::ResourceBinding<t::BindingKey{1, 2}, t::ResourceId{42}>>;
t::ResourceAllocationPlan<Allocation> logical(3, sizes);
t::TypedBundlePlan<Allocation, Map> placement(logical);
auto shared_bytes = placement.bytes(t::ResourceScope::SHARED);
```

`ResourceAllocationPlan::totals()`は従来の宣言数の意味を維持する。共有IDによって削減された実体数は`TypedBundlePlan::buffers(scope).size()`、CPU領域は`total_bytes()`から確認する。payload bytesとCPU Impl bytesを混同しない。

空schema・外部資源だけの群は0 byte・alignment 1。runtime payload数はImplの個数ではない。SHAREDとPER_FRAMEは独立したarenaへ配置する。TRANSIENTのbundle自動生成は今回追加せず、既存の明示TypedBufferArenaで扱う。

## B: bundleと保持の接続

```cpp
using HeapBundle = t::TypedResourceBundle<Allocation, t::HeapStorage>;
using ArenaBundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<Map>>;
ArenaBundle bundle(manager, descriptors, sizes);
bundle.initialize_shared([](ArenaBundle::Shared& shared) {
    shared.buffer<0, 0>().copy(values);
});
auto phase = manager.start_phase<vsl::ComputePhase>();
bundle.update(phase, [&](ArenaBundle::Frame& frame) {
    frame.buffer<2, 1>().copy(frame_values);
});
phase << bundle.get(phase).bind_resources(pipeline).dispatch(4);
phase.submit();
phase.completion().wait();
```

上のbinding番号とpayloadは利用側のAllocationに合わせる。heapとarenaで共通のbundle構築・共有初期化・frame更新・descriptor接続を使用する。arena版の`buffer()`は`PlacedResourceBufferView`を返す。`copy`、`read_into`、部分`view`、`size`、`descriptor`を持ち、backendは解決済みBufferRefとownerを持つBufferViewである。旧backendの`.data_`や直接mappingが必要な利用側は、新policyへの移行時にview APIへ切り替える。共有Bufferとconst frameから直接copyできない型契約は維持する。

各非空群は一度だけCPU領域を確保する。群内の各slotをゼロ初期化し、同じIDの別bindingもその初期値を共有する。宣言・device limitを検証してから構築し、後続のdescriptor／frame構築に失敗した場合も、構築済みのownerを通して巻き戻す。

公開viewとbundle中のviewはarenaを保持するが、CPU領域内のImplはarenaを所有しない。frameからsharedへ所有が向く一方、sharedからframeへの参照は作らない。一つのshared viewはframe arenaを保持しない。

`ResourceRetention::SUBMISSION`はSubmission所有者の寿命まで保持する。`COMPLETION`はGPU完了観測／記録取消でlifetime側のpinを解放する。現在は[保持期間の独立指定](typed-lifetime-boundaries.md)により、heap／arenaともframeの保持、Typed descriptor、外部資源の追加論理制約を同じ既定COMPLETIONへ接続し、旧SUBMISSIONは明示選択する。資源自体が持つ使用状態はどちらのpolicyでも共通で、同期の新しい状態を増設しない。

commandを生存させている場合はcommand自身の保持も残る。GPU完了だけで、利用者が保存したviewやcommandまで無効化しない。native descriptorへの接続にはaliasing shared_ptrを使う移行adapterが残り、descriptor本体のarena化は段階5で扱う。

## C: 実利用

`aho-typed-example`はheapのまま、同じscene／shader／verifyを使う`aho-typed-arena-example`を併設する。両方とも3 frame slot、shared／frame UBO、TypedBatchの2 drawとbind省略、resize時のpipeline再利用を検証する。

```sh
cmake --build build --target aho-typed-example aho-typed-arena-example
./build/bin/aho-typed-example --verify ./build/typed-heap-verification
./build/bin/aho-typed-arena-example --verify ./build/typed-arena-verification
```

残る個別確保はframe wrapper、descriptor／layout／pool、shared所有境界、使用状態内のvector等。VulkanのBufferとdevice memoryもBufferごとに確保する。新しい一括領域がこれらを全て含むとは扱わない。

## 今回の検証と次の境界

Clang20／C++26、macOS arm64／Apple M1、ASan／UBSan、MoltenVK／Vulkan validationで検証した。

- VSL712件中709成功、対応外packed format3件skip。新規はCPU配置契約5件とarena bundleのGPU／寿命契約9件。
- AHO GPU121件成功。Typed compile137件＋AHO TypedBuffer8件の145件成功。heap／arena両方のbundle正常・負例15件ずつと、非trivialな新viewの拒否も含む。
- 変更C++16ファイル／13TUのstrict規約検査と、新viewヘッダー1ファイル／1TUの補完検査が違反0。
- heap／arena両サンプルで3 frame slot／12 submission／2 draw／238831 UBO変更画素／resize 1720×1080→2000×1360／pipeline生成1回。保存した3枚のPNGは両経路でバイト単位に一致。
- installed consumerで公開ヘッダーからbundleの構築・更新経路をbuild／linkし、CPU上の配置計算も実行。異なるartifact IDを持つ同寸法headerとの混在はlink guardで拒否。private Implはinstallしない。
- CMakeの関連targetとMSBuildのXML／新規path／重複登録を確認。生成済み寸法情報とABI定義はCMake targetから受け取る。手書きvcxprojだけで生成処理が完結する対応は追加していない。

Windows／Linux実機、device lost、並行CPUアクセス、性能比較は未検証。CPUアクセスは直列化する。LeakSanitizerは無効化し、weak_ptrの失効と既存の破棄検査を併用した。

今回の到達点は段階1・2と、その実利用検証A〜C。次は段階3のAHO storage世代への接続である。Image／descriptor／pipelineのarena化、Impl集合ヘッダーの資源別分割、全Completionの結果／保持分離はまだ完了していない。

## 段階3の追記

D1〜D3で[AHOのsnapshot・固定import・可変arena世代](../AHO/arena-buffer.md)を接続した。live bindingと世代snapshotを分け、固定importは容量内だけ、可変Bufferは新世代を別arenaへ作る。旧snapshotが新世代を保持し続けないよう、論理UseStateも現在値の所有から分離した。次は段階4のImage／Samplerである。既定heap API、寸法生成の必須方針、CPU ImplとGPUメモリの区別は維持している。

11月7日のレビュー修正で固定importと可変Bufferの公開継承を除去し、snapshotのviewからだけ明示的にimportする契約へ揃えた。factory返却viewのarena ownerもそのまま保存し、Bufferごとのgeneration wrapperによる保持重複を除去した。factory設定は論理状態に同居させ、実体の破棄依存は返却ownerが保持する。Image等へ展開する前提として、物理世代・論理状態・arena ownerの識別を混ぜない。

## 段階4の分割

段階4を、4a: Image／Samplerの共通backendと物理UseState、4b: 実寸法生成と異種arena、4c: Typed schema／bundle接続の順へ分ける。先に[通常APIと指定領域構築の共通処理](image-sampler-backend.md)を確定し、後の配置計画がnative生成や検証を複製せず利用できるようにする。

4aではImage／Samplerのoperationとlifetimeを分離し、device／swapchain所有をImpl外へ置く。通常API・Typed／native descriptor・転送／barrier・framebufferの全経路で同じ物理使用状態を登録する。外部imageはviewだけを所有する。Image／Samplerを利用者向けarenaへ配置するAPI、公開寸法・offset生成、schemaの自動展開は4b以降の終了条件に残す。

4bの[ResourceArena](resource-arena.md)と4cの[Typed owned Image／Sampler](typed-owned-resources.md)で、異種配置とcomponent単位のschema接続を実装した。現在の順序は[完了計画](arena-completion-plan.md)を参照。次は完了結果とin-flight保持の分離であり、descriptor以降の配置とDevice／Instanceは引き続き未完了。
