# Instanceの共通backendと指定領域構築

順序9a。Deviceを必要としないInstance単体のoperation／lifetime backendを実装し、通常生成とCPU指定領域構築を共通化する。PhysicalDevice・Surface・Deviceを合わせる公開StartupArenaは後続で扱う。

## 責務と実体

| 責務 | 実装・契約 |
| --- | --- |
| storage | 実定義から生成した`ImplLayout<VulkanImplAccessor>`／`ResourceSlotLayout`、size／alignment、artifact整合性。指定領域の確保・正しい配置は呼出側 |
| operation | 所有値の生成入力を検証し、Instance／debug messengerを生成・破棄する。native handleへの直接操作と呼出し中のscratchを担当 |
| lifetime | 指定領域のC++実体構築・失敗時rollback、heap rootの所有と正規node、最後のownerによる回収を担当 |
| sync | Instance単体にGPU使用状態は追加しない。現在の子はInstance ownerを保持し、子のNative資源が残る間の親破棄を防ぐ。GPU完了・Device・WSIの契約は後続 |

`VulkanImpl<true/false>`は共通runtime型`VulkanImplAccessor`のaliasとなる。virtual destructorとbool別の動的実体は除去した。Implはowner nodeへの借用、Instance、Messenger、保存したMessenger破棄entryだけを持ち、copy／moveを禁止する。heap rootのnodeは一つ。実行時のboolは生成入力で解決し、破棄は実際に構築したhandleへ従う。

入力のapp名・engine名・extensions／layersは`InstanceCreateInfo`が所有する。native名のpointer配列やvalidationのpNext構造は生成呼出し中だけ存在し、Implへ保持しない。Instance／driver内部の確保はCPU一括領域に含めない。

## 通常APIと配置

```cpp
vsl::InstanceCreateInfo info;
info.application_name = "headless application";
// 実行環境で必要なextensionをinfo.extensionsへ指定する。
auto instance = vsl::VulkanAccessor::Create(std::move(info));
auto owner = instance.owner_ref();
```

この入口はGLFWを呼ばない。既存の`Vulkan<true/false>(app_name, extensions)`はGLFW由来のextensionを収集して同じbackendへ渡すadapterである。GLFW配列がnullかつcount0の場合にポインター演算しない。null extension、空のextension／layer、埋込みNULを拒否し、重複名は順序を保ってまとめる。

`validation`はKhronos validation layerとdebug messengerを有効にする。layerが提供するextensionも照合する。`debug_printf`はvalidationを必要とし、既存`Vulkan<true>`の指定を維持する。[Khronosの初期化仕様](https://docs.vulkan.org/spec/latest/chapters/initialization.html)に従い、`VkValidationFeaturesEXT`をpNextへ接続する。layer settingsへの移行は今回分離し、既存SDKの経路を維持する。

privateの`construct_instance(storage, info)`が`InstanceSlotStorage`へ構築し、通常heap factoryもこれを使う。storageは未構築で、公開slot size／alignmentを満たす必要がある。`instance_at(storage)`は実体へ直接参照し、`destroy_instance(storage)`は構築済み実体にだけ呼ぶ。未構築／二重構築／claimの管理を備えた公開StartupArenaは、親子資源の契約が揃う後続で導入する。既存ResourceArenaはDeviceを前提とするため、今回はINSTANCE kindをそこへ混ぜない。

型寸法は実Implとslotからcompile-onlyで生成し、private実定義とのstatic_assertを置く。公開入力・private slot・target・compiler・構成・標準ライブラリABIをfingerprintへ含め、生成ヘッダーをinstallする。現在は既存15資源kindに加え、Instanceのlayout traitが利用可能になっている。

## 失敗と回収

1. 入力・layer／extensionを検証し、native名の配列を確保する。
2. 仮のImplへInstanceを生成する。SUCCESS以外の出力handleは採用しない。
3. Messengerのcreate／destroy両entryを確認し、生成に成功したhandleだけを仮Implへ保存する。
4. 全部成功してから指定先Implへhandleとentryを移す。heap rootはその後に正規nodeを公開する。
5. 例外時は仮ImplのMessenger→Instanceを回収・null化する。C++ destructorの追加cleanupは冪等となる。

通常の回収もMessenger→Instanceの順。破棄処理はregistryや完了観測をたどらない。保存済みMessenger entryを使い、nodeを持たない旧手動Implだけに互換のentry lookupを残す。`VulkanAccessor::owner_ref()`は正規control block以外を拒否し、同じrootの複数slotでも元ownerを返す。

private `InstanceNativeCalls`は呼出し単位でのみ差替え可能にした。test context／callbackをImplやheap ownerへ保存しない。テストでは実APIへdelegateし、特定生成だけを失敗させ、明示native破棄を観測してからCPU destructorで追加呼出しがないことを確認する。

## 互換性と移行事項

通常のVulkan facade、Accessorコピー、devices()の利用形式は維持する。非公開Implのbool別型・virtual性は変更した。公開`.data_`／`.accessor`は移行用として残し、任意の手動Impl入力は旧入口の信頼条件に従う。正規owner取得は新factoryの実体だけに適用する。

release constructorの`fclose(stdin/stdout/stderr)`／`FreeConsole()`は除去した。標準入出力とconsoleの管理はhostアプリケーションの責務であり、Instance構築からプロセス全体へ作用させない。

PhysicalDeviceは[9b](physical-device-arena.md)でraw Instanceと固定親root／weak getter contextへ移行した。Surfaceは後続で同じ方針へ移す。LogicalDeviceの配置前には、子のshared_from_this取得と全Device固定辺も置換する。OS windowはpluginを含まないownerへ分け、Surfaceとの循環を防ぐ。

## 検証

headless heapと2同型slotの直接配置、正規owner、元facade破棄後のAccessor利用、validationありの生成／破棄順序、Instance／Messenger失敗時の偽non-null出力未採用、同slot再試行、入力検証、二重initialize拒否と破棄の冪等性を検証する。対象Instanceのvalidation callbackはtest collectorでerror0を確認する。driverがない環境は明示skipする。

通常のrelease facadeについてPOSIX descriptor0／1／2の維持も確認した。追加10件、重点12件、VSL951成功・既知packed-format3skip、AHO GPU150／CPU1070、compile195成功。Vulkan validation／ASan／UBSanエラーなし。strictは13ファイル／6TUとcollector追加後の1TUを確認し、違反0。CMake329path、MSBuild／filtersの新5path各一回も確認した。

installed consumer20target、Debug／Releaseの寸法生成・実行、x86_64のcompile-only probeとarchive、異なる構成のheader／binary link拒否が成功した。InstanceのImpl／slotはこの環境で32／32 byte、alignment8。既存15kindの1416 byte計画は不変。private定義／注入口をinstallしていない。heap／arenaの描画とresizeは3frame／12submission／2draw、1720×1080→2000×1360、Pipeline生成1回でPNG3枚が一致した。

失敗はprivate呼出し差替えで実Instanceに対するMessenger生成前のerrorを決定的に作り、回収を観測した。実driverでのOOM・全entry欠落位置・DeviceLost、Windows／Linux実機、LSan、CPU並行利用、性能率は未検証。公開StartupArena、PhysicalDevice／OS window／Surface／Deviceの配置と固定親、既定policy切替・旧入口廃止は後続として残る。
