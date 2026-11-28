# Arena配置の試験導入(Sampler・Buffer・DescriptorSet)

状態: **試験導入。残りの資源へ広げるかは、この文書の評価で判断する。**

#394 は close 予定である。その反省から、同じ目的(通常VSLとTypedが同じ生成処理を使い、Implの配置だけを選べること)を、最小の抽象で試す。

## 何を足したか

`VSL/arena.hpp` の1ファイル(Vulkanに依存しない)だけである。

| 名前 | 役割 |
| --- | --- |
| `Arena` | 寿命を共にする資源のImplを、固定容量の連続したCPU領域へ配置する。構築の逆順に破棄する |
| `place<T>(arena, init)` | heapとarenaで共通の生成入口。`arena == nullptr` ならheapに置く |
| `link(from, to)` | 資源から資源への参照を作る。同じArena内なら非所有、別の所有単位なら所有 |
| `expose(from, to)` | `link`で作った非所有参照を外部へ渡すとき、所有を持つ参照へ戻す |

公開ハンドルの型は変えていない。`SamplerAccessor` などは引き続き `shared_ptr<Impl> data_` を持つ。arenaへ配置したImplのshared_ptrはArena全体と寿命を共有する(aliasing constructor)。

## 新しい型を作らずに済んだ理由

masterには既に次の仕組みがあった。

1. **Implのデストラクタが自分のVulkanオブジェクトを破棄する。** 資源ごとの破棄処理(#394の`destroy_*`)は要らない。
2. **公開ハンドルは`shared_ptr<Impl>`である。** aliasingで所有単位をArenaにできるため、新しいHandle型は要らない。
3. **GPU完了までの保持は`Completion::retain(shared_ptr<void>)`が行う。** arenaの資源を記録すると、Arena全体がsubmissionの破棄まで生存する。配置とは独立したまま、新しい仕組みは要らない。

#394では同じことを `Heap*Owner`、`*SlotStorage`、`construct_*`/`destroy_*`/`*_at` の組を約20資源ぶん手で書いて実現していた。

## 契約

- **寿命**: Arenaと、配置した全資源の`shared_ptr`が消えた時点で、Arena全体を構築の逆順に破棄する。資源を1つだけ先に解放することはない。寿命を揃えたい資源の集まりを1つのArenaにする。
- **失敗**: `init`が例外を送出すると、その資源を破棄して領域を巻き戻す。容量超過は`std::length_error`で、何も構築しない。`init`の中で同じArenaへ`emplace`すると`std::logic_error`になる。
- **同じArena内の参照**: `link`は非所有を返し、循環を作らない。構築時に結んだ参照の先は先に構築されているので、破棄の順序上、デストラクタから使ってよい。構築後に結んだ参照(descriptorのbindingなど)は生存の保持だけに使い、デストラクタから辿らない。
- **外部への受け渡し**: `link`で結んだ参照を公開APIから返すときは`expose`を通す(`Resource::get_pool()`)。
- **スレッド**: `emplace`はスレッド安全ではない。

## 移行した資源と変更量

空白だけの変更(lambdaへ移したことによるインデント)を除いた行数。

| 資源 | 公開API | 実装の変更 |
| --- | --- | --- |
| Sampler | `Sampler(Arena&, device, ...)` | sampler.cpp +11 / −4 |
| Buffer | `Buffer(Arena&, device, size)` | buffer.cpp +18 / −12 |
| DescriptorSetLayout・DescriptorSet | `BindingLayout(Arena&, device, points)`、`Pool::bind(Arena&, layout)` | descriptor.cpp +58 / −26、typed_resources.cpp +3 |

各資源でやったことは同じである。

1. Implの生成を`place<Impl>(arena, init)`に置き換える。
2. 公開コンストラクタに`Arena&`版を1つ足す。
3. 親や束縛先を保持する箇所を`link`に、それを外へ返す箇所を`expose`に置き換える(DescriptorSetのみ)。

既存の呼び出し側と既存テストは変更していない。

### 4つ目の資源(Image)で確かめたこと

「資源を1種類足すとき、触るのはその資源の生成処理とテストだけ」という基準を確かめるため、Imageを別コミットで追加した。

| 資源 | 公開API | 実装の変更 |
| --- | --- | --- |
| Image | `Image(Arena&, device, width, height, ...)` | image.cpp +23 / −16、ヘッダー +13 |

`arena.hpp`と他の資源の変更は不要だった。同じArenaに置いたImage・Sampler・COMBINED_IMAGE_SAMPLERのsetも、循環なしで破棄される。Swapchainが持つImageは対象外で、heapのまま。このコミットは評価用なので、不要なら外してよい。

## 使用例

```cpp
vsl::Arena frame(4096);  // または Arena::bytes_for<Impl>(n) で見積もる
vsl::graphic_resource::BindingLayout layout(frame, device, {{0, Type::UNIFORM_BUFFER, ShaderFlag::COMPUTE}});
auto [ok, set] = pool.bind(frame, layout);
vsl::Buffer<MemoryType::UNIFORM_BUFFER, MemoryProperty::HOST_VISIBLE> uniform(frame, device, 256);
set.update(&uniform);     // 同じArena内なので非所有で結ばれる
// frame・layout・set・uniformが全て消え、GPUの使用も終わった時点でまとめて破棄される
```

## 測定

`test/vulkan/arena_benchmark_test.cpp`(`DISABLED_`のため通常は実行しない)。Release、LLVM 20、MoltenVK、Apple Silicon。7回の中央値で、3回実行して同じ傾向だった。

| 計測 | 個数 | heap | arena | arena/heap |
| --- | --- | --- | --- | --- |
| CPUの配置だけ(Vulkan呼び出しなし) | 10,000 | 231〜245 µs | 134〜152 µs | 0.57〜0.62 |
| Samplerの生成と破棄 | 1,000 | 413〜425 µs | 390〜402 µs | 0.94〜0.95 |
| Buffer(256 B、host visible)の生成と破棄 | 1,000 | 2,575〜2,598 µs | 2,512〜2,547 µs | 0.97〜0.98 |

**読み方:** CPUの確保だけなら約4割速くなる。1資源あたりでは約10 nsの差である。ただしVulkanの生成・破棄を含めると差は2〜6%で、1,000資源あたり数十µsにとどまる。**生成・破棄の性能はarenaにする理由にならない。** arenaの価値は、寿命の揃った資源をまとめて扱えることだけである。記録時にImplを辿るときのキャッシュ局所性は測定していない。

## 判断していただくこと

1. この形で読めるか。語彙は`Arena`・`place`・`link`・`expose`の4つ。
2. 残りの資源(Shader、Pipeline、FrameBuffer、Command、同期、起動資源)へ同じ形で広げたいか。
3. 測定の結果を踏まえ、arenaを続ける価値があるか。続けるなら、Typedのschemaから`bytes_for`を計算し、フレームごとのArenaを作る段階へ進む。

## 既知の制限

- Arena単位でしか解放できない。資源を個別に作り直す用途(resizeで作り直すFrameBufferなど)は、heapか別のArenaに置く。
- 容量はバイト数で指定する。`Arena::bytes_for<Impl>(n)`で見積もれるが、Impl型を名前で指定する必要がある。
- `Pool::bind`の複数版(`bind(n, layout)`、`bind(layouts)`)と、DescriptorPool自体の配置は未対応。
