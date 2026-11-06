# AHO Bufferのsnapshotとarena世代

2026-11-07更新。[VSLの段階的移行](../VSL/arena-migration.md)のD1〜D3。既存GPU_FRIENDLY／ON_GPU／CUSTOMは併設し、arena利用を明示選択する。VSLのprivate ImplやVulkan APIをAHOから直接使用しない。

## D1: live bindingとsnapshot

`BufferBinding`は従来どおり論理Bufferを参照し、現在のsize・storageへ追従する。`snapshot()`はその時点の物理世代を保持する。

```cpp
auto live = buffer.binding();
auto old = live.snapshot();
buffer.reserve(128);
auto current = live.snapshot(); // 新世代
old.read_into(destination);    // 旧世代の内容
```

snapshotはcapacity範囲を表すowning `storage()`、論理`size()`、device、追加論理制約、保持方式を持つ。`read_into`は論理サイズ内へ限定する。`storage()`は明示的な物理範囲への入口で、論理サイズを超える容量部分も含む。一時snapshotからのstorage借用はconst rvalueを含めcompile拒否し、移動元のsnapshotからの資源操作も拒否する。

AHOの論理UseStateを現在世代の所有から分けた。旧snapshot／Typed viewが論理制約を保持しても、新しいstorageまで保持し続けない。GPU使用の物理状態はBufferImplの状態を共用する。AHO経由の旧viewを使用中は、必要な論理更新制限も現在のBufferへ重なる。

Typed uniform／storage／vertex viewはsnapshotから生成する。VSL側の`FromView`でusage・範囲・alignmentを検証し、byte offsetを物理世代の先頭から計算する。通常のVSL Bufferも同じview検証へ接続する。旧Typed内部のaliasing handleは移行adapterとして残る。

## D2: 固定容量import

```cpp
auto imported = aho::resource::import_buffer(arena.view(slot).view(offset, bytes));
imported.assign(initial_bytes);
auto typed = aho::resource::storage_view<std::uint32_t>(imported);
imported.read_into(destination);
```

`ImportedBuffer`はviewのownerを保持する。コピーは論理サイズと現在の資源を共有する。容量は渡されたview範囲で固定し、`reserve`は存在しない。`resize`／`assign`は容量内だけを許可する。親arena wrapperを破棄しても使用可能で、部分viewの外側へアクセスしない。host非可視資源をimportした場合、CPU読み書きは利用できず、GPU転送・Typed bindingに利用する。

内部操作は解決済みBufferRefを使う。`BufferBinding::native()`は既存利用側への互換入口として残し、全物理範囲のviewにだけaliasing accessorを遅延生成する。部分viewをBufferAccessorでは表現できないため、部分importでnative()を呼ぶと拒否する。新しいTyped／転送経路ではこのadapterを通さない。

## D3: 可変arena Buffer

```cpp
auto buffer = aho::resource::make_arena_buffer(device, initial_size);
buffer.assign(initial_bytes);
auto old = buffer.binding().snapshot();
buffer.reserve(new_capacity);
```

`ArenaBuffer`と`ImportedBuffer`は別の公開型で、相互の暗黙変換はない。両者は内部の`detail::ViewBufferBase`をprivate継承し、読み書き・binding・uploadだけを共用する。公開型同士に継承関係はなく、共通利用は`gpu_buffer` conceptで扱う。ArenaBufferのコピーは現在世代とfactoryを含む論理状態を共有し、reserveと拡張可能なresize／assignを持つ。

固定importが必要な場合は、snapshotのowning viewから明示的に作る。

```cpp
auto snapshot = buffer.binding().snapshot();
auto fixed = aho::resource::import_buffer(snapshot.storage());
buffer.reserve(new_capacity);
// fixedはsnapshot時の物理世代・容量を保持する。
```

このimportは独立した論理Bufferを作る。同じ物理世代のUseStateは共用するが、元Bufferの論理サイズや現在世代には追従せず、元Buffer全体の論理制約も引き継がない。`ImportedBuffer fixed = buffer`というコピーや参照による変換はcompile拒否する。

既定では世代ごとに単slot arenaを作る。CPU領域内のBufferImplとUseStateはVSLの共通構築処理を使い、使用中のImplを移動しない。複数slotの既存arenaを拡張したり、同居資源を再生成したりしない。

reserve／resizeの拡張では、候補の構築・host mapping・旧内容のコピー後に現在世代を公開する。assignの拡張では新内容を書き終えてから公開する。例外時は旧実体・論理サイズ・容量・内容を維持する。GPU_FRIENDLYと同じく、新しく増えた領域のゼロ初期化は保証しない。

任意のfactoryを第3引数へ渡せる。返却値は新しいImplを指すowning BufferViewで、同じdevice、要求以上の容量、HOST_VISIBLE、標準AHO Bufferのusageを満たす必要がある。既存の現在Implを別範囲で返して世代切替する指定は拒否する。factoryは旧storageを変更しないこと。

publishは返却されたviewとownerをそのまま保存する。Bufferごとのgeneration ownerへ包み直さず、同じarenaの異なるslotも共通のcontrol blockを保つ。VSLは保持ownerをcontrol blockで重複排除し、各slotの物理UseStateとAHOの論理制約はそれぞれ登録する。操作は解決済みBufferRefへ直接届く。

factoryの設定は論理Buffer状態内に置き、最後のArenaBuffer／live bindingと一緒に解放する。旧snapshotはfactoryや現在世代を保持しない。**返却viewのownerが、資源の破棄に必要な領域・device・allocator等の依存をすべて保持すること**。factoryのcaptureだけで破棄依存を維持する使い方は不可である。外部allocatorを使う場合は、factoryが返す共通arena ownerに依存を持たせ、arenaの破棄後に解放する。AHOは追加の所有階層を補わない。factory自身が特定のarenaをcaptureするなら、そのfactoryが生存する間は当該arenaも保持される。

## 記録と保持

BufferUploadは既存の意味を保ち、invoke時点のsource／destination世代と内容を解決する。保存したuploadの作成後に再確保しても、記録時の現在値を使う。Typed viewは生成時の世代を固定する。CPU GENERAL、既存heap、固定import、可変arenaを混在できる。

旧heap経路は従来どおりSUBMISSION所有者の寿命まで保持する。arenaのAHO転送はCOMPLETION保持へ接続し、GPU完了観測／記録取消後に資源を回収できる。別に保存したsnapshot・view・commandの所有は残る。

TypedDescriptorSet等の既存の保持境界は維持する。完了時回収を選ぶ場合は、そのdescriptorも`ResourceRetention::COMPLETION`で構築する。旧SUBMISSION descriptorやcommandから参照すれば、その所有期間までarenaも残る。

新しい読み取りは呼出中にコピーを終える`read_into`を使用する。旧map／readの生spanは互換APIとして残し、アクセス期間全体を外部同期する。CPU並行アクセスとhost leaseは今回追加しない。

## 残る確保と次の範囲

CPU一括領域に含まれるのはBufferImplと物理UseStateの本体。AHOの論理状態（factoryも同居）、論理UseState、遅延native adapter、使用状態vector、内容保持用の一時CPUコピーは別確保として残る。std::functionのcapture次第では内部確保も発生する。provider／generationの追加control blockは除去した。Vulkan device memoryもBufferごとの確保である。速度向上率は測定していない。

次はImage／Samplerの共通operationと所有・同期を分離し、異種Implの配置へ進む。descriptor／pipeline／presentationと、Impl集合ヘッダーの資源別分割は後続段階で扱う。

## 検証

Clang20／C++26、macOS arm64／Apple M1、ASan／UBSan、MoltenVK／Vulkan validationで確認した。

- D1〜D3初回はVSL712件中709成功、既知のpacked format3件skip。AHO GPU133件・CPU1070件成功。
- 初回はTyped compile137件＋AHO TypedBuffer12件の149件成功。固定importのreserve拒否、snapshotの一時値／const rvalueからの借用拒否、importした資源のpayload不一致を含む。
- 初回の新規はsnapshot寿命1件、固定import4件、可変arena7件。部分範囲、heap／arena／CPU転送、Typed compute、元wrapper破棄後の記録、共有alias、旧世代、移動元の拒否、完了／取消回収を検証した。
- factory例外、容量不足、usage不足、同一Impl再利用、候補へのコピー拒否で旧世代を維持することを確認。11月7日の所有整理では、factory設定の寿命と、返却ownerが保持する実際の破棄依存の寿命を分けて検証した。固定snapshot importの容量不変、暗黙変換拒否、異なるslotのowner同一性・保持1件・個別UseState登録の回帰検証も追加した。
- 変更C++21ファイル／18TUを検査し、修正した4ファイル／12TUと追加GPUテスト2ファイル／2TUを再検査してstrict違反0。最終formatも確認した。
- heap／arenaサンプルのverifyで3 frame slot／12 submission／2 draw／UBO更新／resize／pipeline生成1回が成功。3枚のPNGは両経路でバイト一致した。
- install後の公開ヘッダーだけでsnapshot・import・可変arena・Typed接続・uploadのconsumerがbuild／linkし、空device拒否のCPU検証も成功。private Implはinstallしない。

11月7日の修正後は、AHO GPU136件・CPU1070件・AHO TypedBuffer compile14件が成功。変更C++8ファイル／10TU（GPU条件付きの本体を含む）のstrict違反0。両Typedサンプルのbuildと、再installした公開APIのconsumerのbuild／link・空device拒否も成功した。この修正ではVSL製品コードを変更しておらず、VSL全件と描画画像比較の結果は上記の初回検証による。

Windows／Linux実機、device lost、実際のVulkan生成／mapping失敗の注入、CPU並行操作、性能比較は未検証。Vulkan処理は既存の共通backendを使う。LeakSanitizerは無効化し、weak_ptrと既存の破棄検査を併用した。MSBuildはXML・新規path・重複登録を検査し、生成情報を必要とするarenaのbuildはCMake targetを使用する。
