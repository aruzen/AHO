# LogicalDeviceの共通backendと正規owner

順序9dの実装と受入検証。Instance／PhysicalDevice／Surfaceに続き、通常Deviceと指定CPU領域で同じ構築処理を使う。公開StartupArenaとAHOの起動経路への接続は9eで扱い、順序9全体の完了とはしない。

## 4責務と所有

storageは生成済みDevice Impl／slot寸法と配置先を扱う。operationはPhysicalDeviceの能力／queue／extension／featureを検査し、解決済み実体に対してnative Deviceを生成・破棄する。lifetimeは指定領域へのC++構築、rollback、正規owner、固定親と破棄順を管理する。syncは従来どおりWorkの完了を観測し、Deviceに独立したGPU UseStateを追加しない。

`HeapDeviceOwner → 元PhysicalDevice owner → 元Instance owner`が通常heapの所有経路である。Device rootはSurface／Window／Manager／Workを所有しない。graphics生成でSurfaceを使うのはfamily選択の間だけとし、Deviceの永久親へ追加しない。

LogicalDeviceImplはnodeへの借用、raw PhysicalDevice、保存native Device、実queue roleとenabled featureを持つ。隣接contextは元PhysicalDeviceのweak参照だけを持つ。Device自身のweakは増やさず、既存nodeが持つ正規rootのweakを使う。子の通常操作はraw Device／PhysicalDeviceへ直接届き、所有graphを探索しない。

親getterを呼ぶときだけ、Device nodeから元rootを取得してDeviceImplへaliasする。子Buffer等のrootへDeviceをaliasしない。取得したDeviceだけを保存した場合、子rootを先に回収できる。同じDevice root内の別slotも同じcontrol blockを使う。

Device自身の正規ownerと、子資源を保持する外部ownerは区別する。既存の子Facadeには、元資源を保持する独自deleter等のopaque ownerを使う入口がある。この所有する入口は維持し、非所有aliasと退役済み実体を拒否したうえで、実Deviceの正規ownerを取得する。opaqueな子ownerを正規node付きへ偽装せず、依存graphが内部の隠れた所有を把握できないという既存の契約も維持する。

## 所有する生成入力

`DeviceCreateInfo`はqueue request、priority配列、extension文字列、明示的feature要求、graphics／present roleを値として所有する。空queue配列なら必要なroleのqueue0を作る。graphics familyの省略時は適合するfamilyを選び、headless時のpresentはgraphicsへ揃える。既定はgraphics＋computeを要求する。明示queueはfamilyの重複、queue数、priorityの有限性と0〜1、roleに必要なqueue0を検査する。

`DeviceFeature`はcore feature55種とtimeline semaphoreをuint64 flagsで表す。PhysicalDeviceのsupportedと、Deviceに実際にenabledした値を混同しない。未対応／未知bitを拒否し、指定しなかったfeatureを暗黙に有効化しない。Vulkan型や任意の外部pNext pointerを公開Infoへ含めない。

native生成中だけのscratchがVk queue配列、name pointer配列、core feature構造体、timeline chainを保持する。これらのpointerや入力vectorをImplへ保存しない。portability subsetはadvertiseされた場合に自動で追加する。利用者が任意に無効化するboolは設けない。

Instanceには実際に要求したAPI版とproperties2 extensionの有効状態を保存する。timelineの照会はこれに従ってcore／KHR経路を選び、必要ならdevice側KHR extensionを追加する。PhysicalDeviceのAPI版やentry pointの非nullだけからInstanceの有効状態を推定しない。

```cpp
vsl::DeviceCreateInfo info;
info.features = vsl::DeviceFeature::SAMPLER_ANISOTROPY |
                vsl::DeviceFeature::INDEPENDENT_BLEND;
auto device = vsl::LogicalDeviceAccessor::Create(physical, std::move(info));
auto parent = device.get_physical_device();
// 通常BufferとTypedの資源arenaは、いずれもこのdeviceを受け取る。
```

このCreateはSurfaceなしでも使える。Swapchainを使う場合はextensionとroleを要求し、そのSurfaceへの対応を確認する。既存`LogicalDevice(physical, surface)`はこの入力を作るadapterとして残る。Swapchainは再探索した別familyへ切り替えず、Deviceで実際に作成したfamilyを用いる。

既存SamplerのLINEARはanisotropyを有効にする意味を含むため、実Deviceでそのfeatureが無効ならnative生成前に拒否する。新Createの既定featureはNONEであり、通常のLINEAR Samplerを使う利用者は明示要求する。既存graphics constructorは従来の要求を維持する。validation用stores／atomicsの選択はlegacy template引数に従い、共通backendへグローバルVALIDATION条件を埋め込まない。

## 指定領域と公開前の構築

Device slotはImplと元PhysicalDeviceのweak contextを一括で置く。CMakeが実定義からsize／alignmentをcompile-onlyで生成し、VSL内のactual static_assert、構成別artifact、installの既存経路へ追加する。Deviceのlayout traitを含め19型とし、Device引数が前提のResourceArenaの15資源kindへ起動資源を混ぜない。

private構築はraw PhysicalDeviceを受け取り、指定領域へslot→context→DeviceImpl→native Deviceの順で構築する。成功出力だけを採用し、失敗時は構築済みC++実体を逆順で回収する。公開後のgetter用weak contextは元PhysicalDeviceへ接続する。slotのalignment／容量／構築済み状態は呼出し側の配置ownerが管理する。

子資源の内部構築もraw Device参照を受け取る。これにより、同じ配置owner内のDeviceがまだ公開されていなくても、後続Buffer等を構築できる。生存と依存順は外側ownerの責務であり、この借用を内部から強所有へ変換しない。

```cpp
lifetime::construct_device(device_slot, physical_impl, info);
auto& device_impl = lifetime::device_at(device_slot);
lifetime::construct_buffer(buffer_slot, device_impl, buffer_info);
// 全構築成功後にrootを公開し、node／getter contextを接続する。
// 解放はBuffer → Device → PhysicalDevice → Instanceの依存逆順。
```

この例はprivate backendの契約であり、利用者にImpl完全定義を公開するAPIではない。通常heapとTyped arenaもこのraw構築処理を使用する。旧private Accessor版constructはcanonical ownerを検査してraw版へ委譲する移行adapterであり、native生成・検証処理は複製しない。

## 子資源と固定依存

既存のDevice強所有を持つleaf／BufferArena／ResourceArenaは、その実所有をmanagedな固定Device辺として列挙する。graph表示のためだけに強所有を追加しない。Set→Pool／Layout、CommandBuffer→Poolなどの依存を維持し、Device固定辺を経由する逆循環も構築／更新境界で拒否する。

Descriptor contextは、既存固定Device親のnodeを借用して照合する。Device rootと同じcontrol blockに属する資源はその親で保持済みなので、binding側では借用へ正規化できる。Heap SetはPool／Layout、ResourceArenaは既存Device所有がこのnodeの寿命を保証する。通常の記録やVulkan操作へgraph探索を追加しない。

## 互換入口と残る移行

custom Creatorは正規Deviceを返す経路を維持する。従来の「生成後にPhysical親を書き換える」helperは、正規ownerと元Physicalの一致検査へ変更する。手組みsharedImplを暗黙のsidecarで救済せず、新Createへ移行する。shared Surface overloadはnull／非所有を委譲前に拒否する。

別のwrapperのcontrol blockへDeviceImplをaliasする入口も拒否する。外側wrapperを保持してDeviceを間接的に生存させる形では、正規Deviceの依存nodeと所有identityが一致しないためである。既存Deviceを共有するときはAccessor自体をコピーし、元control blockを維持する。アプリ側の追加状態は別の明示的な所有境界で保持する。

```cpp
auto imported = device; // 正規ownerを共有する。
vsl::graphic_resource::BindingLayout layout(imported,
    vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX);
imported.data_.reset(); // layoutが元Deviceを保持する。
```

public data_／sharedImpl、legacy Creator／private Accessor版constructは移行中の入口である。private raw領域構築を公開StartupArenaへ接続すること、AHOの起動群とWindow群の分離、WSIの厳密完了、arena既定化と旧入口廃止は未完了。CPU slot外のvector／生成scratch、owner境界、driver／GPU memoryの確保も残る。性能率は未測定である。

## 検証結果

Deviceの14件とFramebufferの1件を追加し、同型複数slot、途中失敗・同slot再試行、公開前の子構築、元Device ownerの取得、子rootの独立回収、固定Device辺を経由する循環拒否、同Device rootのBufferをdescriptorで借用する経路、GPU完了／取消保持、queue／featureの入力条件を確認した。既存のopaqueな子ownerは維持し、正規Deviceと異なるcontrol blockへの付け替えは拒否する。

- 対象VSL 115件・AHO 37件、全体VSL 1,012件・AHO GPU 154件／CPU 1,070件、コンパイル契約195件が成功。
- VSLのpacked integer clear 3件は従来と同じdriver capability不足でskip。実GPUはmacOS／Apple M1のMoltenVK、validation layerとASan／UBSanを使用し、LeakSanitizerは無効。
- 規約検査は66ファイル／44翻訳単位で違反0件。製品source登録検査も成功。
- install後の公開ヘッダー利用23 target、Debug／Release、x86_64 compile-only、異なるartifactのlink拒否、一時値からの借用拒否が成功。privateのDevice storage／operation／ownerヘッダーはinstallしない。
- heap／arenaは3 frame slot・12 submissions・2 drawsの描画とresizeを通し、3枚のPNGがそれぞれ一致。形式互換のresizeでpipeline生成は1回。

他OS、異なるqueue familyを必要とするGPU、全featureの組合せ、driverの実OOM／device lostは未検証。CPU slot外の確保数や性能率は測定していない。次の9eで公開StartupArenaとAHOを接続し、WSIの厳密完了と0 extentの扱いは順序10で検証する。
