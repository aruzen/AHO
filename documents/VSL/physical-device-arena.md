# PhysicalDeviceの共通backendと指定領域構築

順序9b。候補列挙・採点のscratchと、選択したPhysicalDeviceの永続実体を分ける。通常VSLと指定CPU領域への配置は共通backendを利用する。公開StartupArena、Surface／OS window、LogicalDeviceの配置は後続である。

## 4責務と所有経路

| 責務 | 実装・契約 |
| --- | --- |
| storage | `ImplLayout<PhysicalDeviceImpl>`と`ResourceSlotLayout`で実寸法を提供する。slotにはImplとweak getter contextを含める。領域の確保・alignment・未構築状態の保証は呼出側 |
| operation | native候補の列挙・出所照合・能力照会・既定採点、解決済み候補からの初期化を担当する。通常操作はraw Instance／native PhysicalDeviceへ直接届く |
| lifetime | slot上のC++構築／破棄、初期化失敗のrollback、heap rootの正規ownerとInstance固定親を担当する |
| sync | PhysicalDeviceには独立したGPU UseStateを設けない。native handleはInstanceに属する借用であり、Instance固定親によって失効を防ぐ |

通常heapの経路は`PhysicalDevice facade → HeapPhysicalDeviceOwner → Instance owner`である。rootはDependencyNode、固定親OwnerRef、slotを一つの所有単位に置く。Implにはnodeへの借用、raw Instance、native handle、能力cache、隣接contextへの借用を置き、copy／moveを禁止する。contextは元InstanceのWeakOwnedRefだけを持つ。内部から自rootへの強い所有は作らない。

`get_instance()`はweak contextから元Instanceの正規control blockを取得する。子PhysicalDeviceのownerへaliasしたInstanceを返さない。取得したInstanceだけを保存すれば、PhysicalDevice実体は先に回収できる。Device側は今回、従来の`parent_device`へ新PhysicalDevice rootのaliasを保持する。Device自身のraw親／固定辺への移行は9dで行う。

## 通常APIと候補の互換性

```cpp
vsl::InstanceCreateInfo info;
// 利用するdriverに必要なextensionを指定する。
auto instance = vsl::VulkanAccessor::Create(std::move(info));
auto physical = vsl::PhysicalDevice::Select(instance);
auto name = physical.name();
auto original_instance = physical.get_instance();
```

`Select()`はnative handleとproperties／featuresの値をscratchで採点し、勝者だけをheapへ構築する。既定`PhysicalDevices<PhysicalDevicesSercher>::search()`も同じ採点・構築処理を使う。geometryShaderがなければ0点、discrete GPUへの加点、maxImageDimension2Dによる採点を維持し、int範囲外の合計だけを飽和させる。0点は候補となり、同点では先の候補を選ぶ。

既存`get()`とcustom searcherは、候補を外部へ保存できるowning APIのまま維持する。custom benchmarkへ渡した候補と返す勝者のImpl identityを保ち、採点後に別Implを作り直さない。負点の拒否と最初の同点候補優先も維持する。候補一覧はhost側のsnapshotであり、配置資源kindへ追加しない。

列挙snapshotの公開`data_`を経由する旧入口は、呼出し時にsnapshot、Instance owner、handle配列の値を確保してからnative照会へ進む。native handleはsource Instanceが列挙したものかを、能力照会前に検査する。任意の生pointerが有効かを判定する機能ではない。手動`shared_ptr<Impl>`構築と公開`data_`は順序12で廃止する移行入口であり、既にダングリングしたpointerの安全性を保証しない。

## 指定領域と構築手順

private backendは次の形で共用する。

```cpp
auto candidate = operation::select_physical_device(instance_impl);
lifetime::construct_physical_device(slot_address, instance_impl, candidate);
auto& impl = lifetime::physical_device_at(slot_address);
// owner公開後にnodeと元Instanceのweak getter情報を接続する。
// 最後の利用後だけ呼ぶ。
lifetime::destroy_physical_device(slot_address);
```

candidateはraw originを含む借用scratchである。採点から構築まで元Instanceを保持し、未公開StartupArenaの外へescapeさせない。構築時は同じInstanceと解決済みcandidateを照合し、再列挙せずproperties／featuresを値として採用する。memory propertiesは従来どおり必要な時点で取得する。能力cacheの並行更新への新しい同期保証は追加しない。

構築順はslot→context→Impl→初期化。例外時は構築済みImpl→context→slotを回収する。PhysicalDevice単体にはnative destroyがなく、CPU実体を回収した後に外側rootのInstance固定親を解放する。raw backendは未構築かつ正しいsize／alignmentの領域を要求し、二重構築のclaimは将来のStartupArenaが管理する。構築済みImplへの二重initializeは拒否する。

複数の同型slotは、領域先頭と各offsetから直接参照する。各Implを別ownerで包み直さず、一つのrootの正規ownerを共有できる。同じ起動root内へInstanceも配置する後続では、内部の親は借用に正規化する。全nodeの公開／slotへの設定後にweak getter contextを接続し、未公開のrootへ旧custom callbackを渡さない。

CPU領域へ含むのはImplとweak contextである。列挙vector、採点scratch、ownerの外部control block、driver内部確保、LogicalDeviceや子のVulkanメモリは別に扱う。今回の配置でVulkan device memoryを一括確保したとは扱わない。

## 寸法と配布

実定義からcompile-onlyでImpl／slotのsize・alignmentを生成し、VSLの実定義とのstatic_assertを追加した。対象compiler・target・構成・標準ライブラリABI・公開入力・private slotをfingerprintへ含め、生成ヘッダーをinstallする。異なるartifactのheader／library混用は既存link guardで拒否する。

Device前提のResourceArenaへstartup kindを混ぜない。既存15資源kindのまま、InstanceとPhysicalDeviceのlayout traitを追加した計17型の寸法情報を提供する。公開StartupArenaは後続でこれらを合成する。

## 検証と後続

同型2slotの直接参照と共通root／固定親、元Instance ownerを返すgetter、facade破棄後のcustom候補とLogicalDeviceの利用、能力cache、foreign Instance／偽owner／空候補の拒否、途中失敗後の同slot再利用を検証対象にする。fixtureも正規Instance／PhysicalDevice factoryへ移し、Instance生成時pNextと破棄時を含むvalidation collector、既存skip条件・feature chainを維持する。

追加11件、重点19件が成功した。MoltenVK／Apple M1上でVSL962成功・既知packed-format3skip、AHO GPU150／CPU1070、compile契約195成功。validation／ASan／UBSanエラーはない。strictは変更14ファイル／7TUと、最終のdevice.cpp／テストの各1TUを確認し、違反0。CMake333pathとMSBuild／filtersの新4pathの一意な登録も確認した。

installed consumer21target、Debug／Releaseの寸法生成・実行、x86_64のcompile-only probe／archive、異なる構成のheader／binary link拒否と既存の一時値借用拒否が成功した。PhysicalDeviceのImpl／slotはこの環境で1616／1640 byte、alignment8。既存15資源の1416 byte計画は不変で、private Impl／構築入口はinstallしない。heap／arenaの描画・resizeは3frame／12submission／2draw、1720×1080→2000×1360、Pipeline生成1回を維持し、PNG3枚が一致した。

実driverのOOM／DeviceLost、Windows／Linux実機、LSan、CPU並行利用、性能率は未検証である。privateのcandidate出所不一致と同slot再試行は決定的に検査したが、driver内部の失敗を網羅したものとは扱わない。

次はpluginやEngineを持たないOSWindowOwner／GLFW leaseと、Surfaceの共通backend／固定親／CPU配置へ進む。logical closeとnative windowの最終破棄を分け、WindowData→Surface→WindowDataの循環を作らない。その後Deviceの共通backend、子のshared_from_this置換、全heap leaf／ResourceArenaのDevice固定辺、公開StartupArenaとAHO接続へ進む。

残る個別確保は通常heap root、旧custom searcherの各候補、列挙snapshot／vector、Device以降の移行前資源である。操作時の親所有階層をたどる処理はraw参照に置き換えたが、公開所有取得境界では正規owner検査と参照カウントを使う。確保数／時間／性能改善率は未測定である。
