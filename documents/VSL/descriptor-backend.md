# Descriptorの共通構築backend

[完了計画](arena-completion-plan.md)の5b前半。5aの[共通使用状態](descriptor-state.md)を維持し、Layout／Pool／SetのCPU領域とnative生成・破棄を分離する。5b後半の[ResourceArena接続](descriptor-arena.md)で公開配置と生成寸法も提供する。

## 生成・操作・所有

- `DescriptorLayoutCreateInfo` はbinding宣言、`DescriptorPoolCreateInfo` は型ごとのdescriptor容量とmaxSetsを持つ。通常ManagerのmaxSets=200と既存の型別fallbackは維持する。
- operationは解決済みImpl／native DeviceへLayout／Pool生成、複数Setの一括確保、更新・破棄を行う。領域や共有所有を選択しない。Setの一括確保は同じLayoutの複数利用を許容するが、同じtarget Implの重複をnative呼出前に拒否する。
- lifetimeの `construct_descriptor_layout/pool/set` が指定領域にImplを構築する。構築失敗時はnativeが作成済みなら破棄し、CPU実体も一回だけ破棄する。Set内のUseStateはImplの一部であり、別領域に重複配置しない。
- 通常VSLも同じbackendを使う。HeapDescriptorLayoutOwner／PoolOwnerがDevice ownerを保持し、HeapDescriptorSetOwnerがPool／Layoutのownerを保持する。Impl内のDevice／Pool／Layoutは借用参照とnative handleにする。

通常操作では、Setのnative Deviceを直接使う。Set→Pool→Deviceの共有所有を解決してnative Deviceを探す処理は不要になる。

`Resource::get_pool()` はPoolに記録したweak ownerから正規の所有を取得する。返却値がSet ownerを保持することはなく、元Poolとのcontrol blockの同一性を維持する。Set自体の最後のownerを解放した後も、返却Poolから新しいSetを生成できる。

## 一括生成の失敗

heapのbulk factoryは、各SetのCPU実体・外部依存owner、戻り値vector、Poolのweak registry容量を先に準備する。native一括確保に成功した後は、確保や例外を伴わずhandleを割り当ててweak登録する。

native一括確保で一つでも失敗した場合、Vulkanは当該呼出の作成済みSetをすべて破棄し出力をnullにする契約である。そのため失敗時は準備済みCPU実体だけを回収する。[Khronos仕様](https://docs.vulkan.org/refpages/latest/refpages/source/vkAllocateDescriptorSets.html)

private bulk factoryはallocation operationを関数引数として受け、既定で共通Vulkan operationを使う。検証では同じ境界に失敗結果を注入し、registryとowner数が変わらず再試行できることを確認する。これは実GPUのpool枯渇を発生させた検証とは区別する。

## 下位の配置契約

領域は未構築であり、各DescriptorSlotStorageのsize／alignmentを満たす必要がある。Device／Pool／Layoutの保持は呼出側が担当する。Setに対する `prepare_descriptor_set` はCPU実体だけを構築し、native未確保でもdestroyできる。

公開前に、各Setのowner aliasをPoolのweak registryへ登録する。単体のconstructはownerを選ばないため登録しない。今後のarena factoryもこの登録を行い、明示Pool.destroyのidle検査とmetadata無効化へ接続する。owner取得は公開境界だけで行い、rollbackやroot破棄は借用参照で処理する。

destroyは使用完了／取消を確認した後に、構築済みの実体へ一度だけ呼ぶ。具体的なnative破棄はImplのdestructorから共通operationを呼び、CPUコンテナより先に行う。外側ownerの依存はその後に解放する。[native Set解放の使用完了条件](https://docs.vulkan.org/refpages/latest/refpages/source/vkFreeDescriptorSets.html)

## 次の接続と残る確保

次に3種類の実寸法・slot計算を公開し、ResourceArenaの依存順構築・逆順rollbackへ接続する。immutable Layout／PoolにGPU UseStateは追加せず、Setは内蔵状態を使用する。可変長コンテナと生成scratch、通常heapのownerごとの確保は残る。

arena Setでは、同rootへの所有を借用へ正規化する必要がある。5bではowner contextを各arenaに一つ置き、構築時の固定依存だけを認める。5cで可変の外部依存とTyped schemaへ広げる。CPU配置と一括破棄の寿命群は引き続き分ける。

5b後半で固定依存contextとResourceArena配置を公開した。可変の外部依存とTyped自動配置は5cに残る。性能改善率は未測定である。

## 検証

指定領域の2つのSet、重複target拒否、失敗後の領域再利用、allocation失敗注入後のowner／registry不変とGPU再試行、Poolの正規control blockとSet回収を新GPU3件で確認した。既存のDevice保持テストも、独立owner tokenのweak観測へ強化した。

macOS arm64／Clang20／ASan・UBSan／MoltenVK 1.4.2・validationで、VSL775件中772成功・既知3skip、AHO GPU140件・CPU1070件、compile契約175件成功。strictは変更C++12ファイル／8TUで0、最終の契約・移植性修正3ファイル／5TUも0。CMake274パス、MSBuildとfiltersの新3パスを検証した。

installed consumerのbuild／link／CPU実行、新create-info提供とprivate Impl非公開、Debug／Releaseの実寸法検査、同寸法で異artifactのlink拒否、x86_64 cross archiveを確認した。両サンプルの3frame・12submission・2draw・238831更新画素・resize・pipeline生成1回とPNG3枚一致も確認し、VUID／sanitizer errorはなかった。

LSan、Windows／Linux実機、CPU並行操作、実DeviceLost、全native失敗位置への注入と性能率は未検証。driverの最終メモリ表示をリーク不在の根拠にはしていない。
