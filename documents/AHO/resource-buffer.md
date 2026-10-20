# AHOのBuffer

AHOは通常のCPU/GPUデータ管理と転送を提供する。メモリ特性・使用用途・割当方法などの専門的な制御にはVSLを直接使い、必要ならCustomでAHOの操作へ接続する。

## 標準型

| 型 | 保持するもの | 主な操作 |
| --- | --- | --- |
| `Buffer<General>` | 通常のCPUバイト列 | data/bytes、reserve/resize、assign/write |
| `Buffer<GPUFriendly>` | HostVisibleなVSL Bufferと共有マップ | read/write/map、reserve/resize、assign、upload |
| `Buffer<OnGPU>` | GPU利用向けのVSL Buffer | native、upload。サイズは構築時に指定する |

各Bufferのコピーは、内容・論理サイズ・容量・GPU使用状態を共有する。移動元のsize/capacityは0となり、変更操作は空状態の例外を返す。CPUのGeneralにはGPU使用状態はない。

GPU用の標準Bufferは、転送元・転送先・頂点・インデックス・Uniform・Storageのusageを持つ。GPUFriendlyはHostVisibleを要求し、HostCoherentを優先する。coherentでなければVSLのread/writeがinvalidate/flushを行う。OnGPUはDeviceLocalを要求する。UMA等で実際のメモリがHostVisibleでも、OnGPUにはCPUアクセスAPIを公開しない。

sizeは論理バイト数、capacityは要求した確保バイト数。コンストラクタのsizeは両方の初期値である。Generalはゼロ初期化するが、GPUメモリの初期内容は未定義である。GPUFriendlyのresizeも追加領域を初期化しない。assignはデータと論理サイズを置き換え、容量を小さくしない。reserveは既存の論理範囲を保持する。OnGPUのサイズ変更はこのAPIに含めず、専門的な再構築にはVSLを使う。

Generalの従来data()はvoid*として維持し、const版とbytes()によるspanを追加した。spanは再確保で無効になる。GPUFriendlyのread()もBufferの再確保前までのviewであり、利用中はBufferを保持する。map()は独立したマップviewを返す。標準のVSL holderは元のBufferが解放されてもストレージを保持する。

## Upload

```cpp
#include <AHO/resource/buffer.hpp>
#include <VSL/vulkan/phase.hpp>

using namespace aho::resource;
Buffer<BufferType::General> cpu;
cpu.assign(bytes);
Buffer<BufferType::OnGPU> gpu(device, cpu.size());
auto phase = manager.startPhase<vsl::ComputePhase>();
phase << gpu.upload(cpu);
auto completed = phase.completion();
phase.submit();
completed.wait();
```

uploadはコマンドを作り、invoke時の内容を記録する。自動submitやCPU待機は行わない。RenderPassの外で使う。転送先の論理サイズを超える書き込みは例外となり、転送先のサイズは変えない。

Generalからの転送は、転送先が保持するGPUFriendlyのstagingを使う。容量不足時だけ再確保し、後続の転送でも再利用する。同一stagingの書き換えを防ぐため、同じ転送先へのGeneral uploadが未完了なら次のGeneral uploadは拒否する。同じPhase内で複数の範囲を転送するときは、先にGPUFriendlyへ全データを書き、uploadのsourceOffset/bytesを指定する。

GPU Bufferからは`destination.upload(source, destinationOffset, sourceOffset, optionalByteCount)`でGPU転送する。sourceを読み直してCPUへコピーする処理は挟まない。sourceにはGpuBuffer conceptを満たす標準型・Customを渡せる。

使用中はassign/write/read/map/reserve/resizeを拒否する。busyはPending/Submittedでtrue、waitは未送信ならfalse、送信済みなら指定timeoutで待機する。同じPhaseでの複数転送は許可するが、その間にCPUから内容を変更してはならない。キャンセル後は再利用できる。

同じResourceへの並行したCPUアクセスは呼び出し側で同期する。AHOの使用状態はAHOのupload/Texture更新によるものを追跡する。native()や取得済みのmapから直接操作する場合、以後のCPU/GPUアクセスの同期は呼び出し側で管理する。

## Custom

```cpp
using Native = vsl::Buffer<vsl::MemoryType::TransferSource,
                           vsl::MemoryProperty::HostVisible>;
auto custom = aho::resource::make_custom_buffer(Native(device, bytes.size()));
custom.assign(bytes);
auto next = manager.startPhase<vsl::ComputePhase>();
next << gpu.upload(custom);
next.submit();
```

StorageとCustomizationの型を保った`Buffer<Custom, Storage, Customization>`を返す。Storageは値として保持できるVSL BufferAccessor派生型である。Storageを指定しないBuffer<Custom>は未定義とし、空の型として誤って使えないようにする。

カスタマイズはメンバーを持つオブジェクトで渡す。型で解決し、仮想インターフェースやstd::functionへ置き換えない。

```cpp
struct Customization {
    vsl::LogicalDeviceAccessor device;
    auto map(Native& storage) { return storage.data(); }
    Native reallocate(const Native&, std::size_t capacity) {
        return Native(device, capacity);
    }
};
auto custom = aho::resource::make_custom_buffer(
    Native(device, 64), Customization{device});
custom.reserve(128);
```

- map(storage)は、read(byteCount, offset)とwrite(span, offset)を持つマップを値で返す。CPU/GPU間の可視性と対応するストレージの範囲を正しく扱う契約とする。標準のVSL holderをそのまま返せる。
- mapを省略した場合、HostVisibleを要求するVSL型には標準のdata()を利用する。それ以外にはCPUアクセスAPIを生成しない。
- reallocate(oldStorage, capacity)は、同じdevice上で要求容量以上の新しいStorageを返す。コピー可能なCustomizationとCPUアクセスが揃う場合だけreserve/resizeを提供し、既存データの保存はAHOが行う。再確保失敗時は元のResourceを維持する。
- mapだけの型、カスタマイズなしの型、move-onlyなCustomizationも利用できる。固定容量の型のassignは、既存容量内でのみ利用できる。
- native()は保持するVSL Bufferを元の型で返す。基礎となるVSLハンドル自体の差し替えは、AHOのマップと状態を無効にするため行わない。

BufferBindingは転送に必要な参照と使用状態を共有する。AHOの各処理はGpuBuffer conceptとこのbindingを利用し、Custom専用の転送実装を追加しない。SubmissionはStorage・Customization・マップを含むResourceを保持する。Completionを持つ管理状態はSubmissionへ保持せず、循環を避ける。
