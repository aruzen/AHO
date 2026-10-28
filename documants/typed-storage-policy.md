# Typed Resource StoragePolicy

## 不足と変更範囲

TypedResourceBundleは宣言から生成する型をTypedResourceBufferへ固定しており、AHO／Customをowned資源として選べなかった。既存のSchema・constructor・Frame／Sharedの型付きAPIを既定policyで保ち、生成するbuffer型とfactoryだけを差し替える。

CPU allocator、arena、placement new、device-local資源のupload自動化は後続の段階。このPRではホストからcopyできる資源を対象とする。

## 契約

- TypedResourceBundle<Schema, Policy>。Policy省略時はDefaultResourceStoragePolicyを使う。従来のmanager／descriptor manager／sizes／external_resourceの呼び出しは変更しない。
- 状態を持つpolicyを渡す場合は、policyを第一引数とするconstructorを使う。既存の`{}`によるsizes指定と曖昧にならない。
- PolicyはBuffer<Binding, Allocation>とmake_buffer<Binding, Allocation>(device, count, array_element)を持つ。Bindingはpayload／kind／stage、Allocationはset／binding／scopeを提供する。
- Bufferはmove構築可能で、型付きcopy(span<const Payload>)、size()、descriptor()を提供する。move代入は要求しない。payload／kind／factoryの戻り型／copyの型をcompile時に照合し、生成後の要素数・descriptor range・所有者・使用状態も検査する。
- Policyは構築中だけ使う。返す各owned資源は独立して更新できるbuffer、または重ならない領域とし、必要な所有者を自分で保持する。Policyオブジェクトへの借用参照を残さない。
- ExternalAllocationはpolicyによる生成の対象外。共有／フレーム別の生成数、初期化・更新・GPU完了までの保持は既存bundleの契約を維持する。
- AHOのTypedStoragePolicyはHostBufferFactoryを既定とし、Custom factoryから返されたAHO Bufferも型付きwrapperで接続する。wrapperはpayload領域をゼロ初期化し、copy／view／descriptorを提供する。ゼロ化は固定4KiBのbyte blockを使い、payloadと同サイズの一時vectorは作らない。
- backend()は従来同様のescape。native handleや論理範囲を変更してはならない。AHO wrapperはcopy時にstorage差し替えを検出する。
- planのbuffer数とpayload bytesは宣言上の論理量であり、policyが使うVulkan allocation・CPUメモリ・suballocationの物理量ではない。

既定APIを使う既存サンプル・compile契約・実GPUテストを維持し、AHO／Custom・共有／フレーム別・配列・例外解放・GPU使用中の更新拒否を追加検証する。

## 使用例

```cpp
#include <AHO/resource/typed_storage_policy.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>

// Allocationは従来のResourceAllocationSchemaをそのまま使う。
using Policy = aho::resource::TypedStoragePolicy<>;
using Bundle = vsl::typed::TypedResourceBundle<Allocation, Policy>;
Bundle resources(context.commands(), context.descriptor_manager(), sizes);
resources.update(phase, [&](Bundle::Frame& frame) {
    frame.buffer<0, 0>().copy(std::span(&parameters, 1));
});
phase << resources.get(phase).bind_resources(pipeline.bind_vertices(vertices)).draw(3);
```

Customの生成を指定する場合は、factoryが`make_buffer<Binding, Allocation>(device, bytes, array_element)`からAHO Bufferを返す。VSLのPolicyへ渡すcountはレコード数だが、AHO factoryへ渡すbytesはバイト数。必要なStorage／Customizationの型をそのまま保つ。

```cpp
using Policy = aho::resource::TypedStoragePolicy<MyFactory>;
using Bundle = vsl::typed::TypedResourceBundle<Allocation, Policy>;
Bundle resources(Policy{MyFactory{pool}}, commands, descriptors, sizes);
```

Policyは構築中だけ使い、update／bind／drawへ仮想呼び出しやstd::functionを追加しない。Frame／Shared／資源の所有方式はこの段階では変更しない。

## 検証結果

最新masterへrebaseした状態をmacOS／Clang 20／MoltenVKで検証した。Windows上の実行検証は未実施。

- ASan／UBSan／Vulkan validationを有効化。AHO CPU 1,068件、AHO GPU 112件、VSL 675件成功。VSLのpacked format 3件はGPU非対応のためskip。
- 関連compile契約137件成功。既定型・旧constructor・外部資源の互換性、誤ったpayload／kind／factory／copyの拒否を確認。
- 新規GPUテスト9件を含み、move-only factory・move代入不可buffer、共有／フレーム別／descriptor配列、3枠・12送信、使用中の更新拒否、構築失敗時の解放、bundle破棄後のCustom所有者保持を確認。
- strict styleは11ファイル／8 TU、違反0件。
- 既存Typedサンプルで120フレーム描画、UBOによる描画変化、resize時のpipeline維持を確認。
