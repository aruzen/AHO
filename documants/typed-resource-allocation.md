# Typed資源の割当宣言と生成

## 発見した不足

TypedFrameResourcesは任意の型付き資源構造体を管理するが、bufferの生成・descriptorの生成・writeによる接続は利用側のconstructor／Factoryに残っている。ResourceSchemaだけでは、buffer内部の要素数・フレーム枠ごとの独立性・外部資源との共有方法は確定できない。

既存のResourceSchemaをshaderとの契約として維持し、別の割当schemaでこれらを宣言する。stackは割当schemaと計画、資源の自動生成と接続、Sample適用の3段階とする。

## 割当schema

- `ResourceAllocationSchema<Resources, Allocations...>`はshaderの全bindingをちょうど1回ずつ指定する。不足・重複・未知のbinding・画像へのbuffer割当・不正なscopeはcompile時に拒否する。
- `BufferAllocation<set, binding, FixedElements<N>, ResourceScope::PER_FRAME>`で各descriptor要素につきNレコードのbufferを作る。既定は1要素、PER_FRAME。
- `ResourceScope::SHARED`はbufferを全枠で共有する。同じsetにPER_FRAMEのbindingがあればsetは枠ごと、全bindingが共有ならsetも1つを共有する。
- `RuntimeElements`は`ResourceAllocationSizes<Schema>::set<set, binding>(count)`で実行時の要素数を指定する。未指定・0は計画時に拒否する。
- `ExternalAllocation<set, binding>`は既存のTyped資源を接続する。画像サイズや初期layoutなどは既存の資源作成・同期APIで指定する。
- descriptorの配列数はResourceSchema、buffer内部の要素数は割当schemaが持つ。配列の各bufferは同じ要素数で、別々に生成する。

`ResourceAllocationPlan<Schema>`はGPUを使わずにbuffer数、descriptor set／descriptor数、論理payload byte数を計算する。固定容量ならconstexprでも利用できる。積と合計のoverflowは検出する。

## メモリ計画の境界

payload byte数はC++のsizeofと要素数の積。Vulkan memory requirements、alignment、専用allocation、descriptor poolのdriver内部メモリ、ホスト側pimplの確保量は含めない。今回は既存の所有方式で生成し、arena・placement newは後続の段階とする。

## 宣言からの自動生成

`TypedResourceBundle<Allocation>`はCommandManagerとdescriptor managerを受け取り、割当plan、共有資源、フレーム資源、setの生成と接続をまとめる。要素型・buffer usage・descriptor配列数は既存ResourceSchemaから取得し、二重に指定しない。

```cpp
using Allocation = t::ResourceAllocationSchema<Resources,
    t::BufferAllocation<0, 0>,
    t::BufferAllocation<1, 0, t::FixedElements<1>, t::ResourceScope::SHARED>>;
using Bundle = t::TypedResourceBundle<Allocation>;
Bundle resources(context.commands(), context.descriptor_manager());
resources.initialize_shared([&](Bundle::Shared& shared) {
    shared.buffer<1, 0>().copy(std::span(&palette, 1));
});
resources.update(phase, [&](Bundle::Frame& frame) {
    frame.buffer<0, 0>().copy(std::span(&parameters, 1));
});
const auto& frame = resources.get(phase);
phase << frame.bind_resources(pipeline.bind_vertices(vertices)).draw(3);
```

- owned bufferはHOST_VISIBLE | HOST_COHERENTでpayload bytesを0初期化する。`TypedResourceBuffer(device, std::in_place, count)`を追加し、payload型が整数でも値とのoverloadを曖昧にしない。ホスト側に同サイズの一時vectorを作らず、mappingへ直接書き、既存のflushを呼ぶ。
- 全資源を生成してsetへ自動接続する。疎なset番号、複数set、descriptor配列に対応する。`frame.set<set>()`はconst参照、`frame.bind_resources(binding)`はResourceSchema順に全setをbindする。
- フレーム用bufferの変更は`update`内で`frame.buffer<set, binding, array_element>()`を使う。`get`後のbuffer、共有buffer、setはconstのため通常APIから更新できない。
- 共有bufferは`initialize_shared(callback)`で最初のフレーム利用前に1回だけ初期化できる。再入・フレーム開始後の初期化・二度目の初期化は拒否する。callbackの失敗後は部分初期化状態を利用させず、bundleを再作成する。
- `RuntimeElements`の容量は第三引数のResourceAllocationSizesへ渡す。planの確定とdeviceのdescriptor range上限の検査は実buffer生成前に行う。構築途中の例外は既存の所有者によって巻き戻す。
- 外部資源は`external_resource<set, binding>(typed_resource)`、descriptor配列はstd::arrayで渡す。全ExternalAllocationへの入力が必要で、種別・payload・配列数・不足・重複・余分な入力をcompile時に照合する。device・usage・offset/rangeは既存TypedDescriptorSetの検証を通す。
- 資源とsetはFrame／SharedStateが保持し、使用したFrameは既存TypedFrameResourcesが送信完了まで保持する。bundleとFrameのcopyは禁止、moveは許可する。

### 同期と所有の契約

update／initialize_sharedの参照や可変bufferのaliasを保存し、後から書き換えてはならない。callback内では記録・submit・cancelを行わず、資源の更新に限る。getの参照と、それを使うcommandは取得したPhase内で使用する。共有初期化後のCPU書き換えは行わない。C++側の外部aliasやbackend escapeを封じるものではない。

SHAREDは共有の宣言であり、GPUのbarrier挿入ではない。共有storage bufferや外部画像へのGPU書き込み、画像layout遷移は既存の同期APIで管理する。画像の寸法・upload・device-local bufferへの転送は今回は自動化せず、ExternalAllocationで接続できる。

### ファイル構成

- typed_resource_allocation.hpp: 宣言・容量・GPUを使わない計画。
- typed_resource_bundle.hpp: 利用側APIとFrameの型付きaccessor。
- typed_resource_bundle_detail.hpp: tupleからのbuffer／set生成と自動接続、外部入力の型照合。
- typed_resource_bundle.cpp: device依存の事前検証。

## 検証

- 割当計画: LLVM 20 / C++26 / DebugでVSLとテストをビルド。CPUテスト5件・コンパイル契約11件成功。変更C++ 3ファイル／2TUのformat・命名・配置検査は違反0件。
- Debugビルド中に環境の空き容量不足が発生した。並列数を6から2へ下げて再開し、ビルド完了。製品コードや他の作業ファイルの削除は不要だった。
- 自動生成: MoltenVK 1.4.2とvalidation layerでGPUテスト9件、コンパイル契約15件成功。3枠／12送信、疎なset・共有と独立性・UBO/SSBO・descriptor配列・外部texture配列・失敗時の巻き戻しを検証。skip・validation errorなし。
- 自動生成層の変更C++ 6ファイル／3TUのformat・命名・配置検査は違反0件。
