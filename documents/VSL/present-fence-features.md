# Present Fenceの能力要求と有効化

順序10aの実装と検証。今回の対象はmaintenance1の能力照会と有効化であり、Fence付きpresentの記録・完了観測は10cへ残る。`REQUIRE_FENCE`を指定して起動できたことだけでは、現在のpresent経路の厳密な資源回収を保証しない。

## 要求と実能力

`DeviceFeature::SWAPCHAIN_MAINTENANCE1`は、実Deviceで有効にするfeatureの要求である。PhysicalDeviceのsupported、Instanceで有効にした依存、実Deviceのenabledを別々に扱う。Device extension名の指定だけではfeature bitを立てない。

現SDK1.4.309.0にはEXTの構造体・定数があるため、この実装はEXT経路を使う。実GPU runtimeはMoltenVK1.4.2であり、SDK同梱driverとは異なる。KHRだけを提供する環境への対応は今回の対象外で、Vulkan API1.4という理由だけで拡張要求を省略しない。

InstanceはKHR_surface／KHR_get_surface_capabilities2／EXT_surface_maintenance1を必要とし、API1.0ではfeatures2KHRも有効にする。DeviceはKHR_swapchain／EXT_swapchain_maintenance1とfeature=trueを要求する。依存不足・未対応はnative Device生成前に拒否する。[EXT surfaceの依存](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_surface_maintenance1.html)、[EXT swapchainの依存](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_swapchain_maintenance1.html)

## 公開入口と互換

`PresentFencePolicy {LEGACY, REQUIRE_FENCE}`を生成時の独立した要求として置く。heap／arenaの配置選択や、記録保持のLifetimePolicyへ結び付けない。既存の生成入口とAHO既定はLEGACYを維持する。

`glfw_instance_info(info, policy)`は必要なInstance拡張を所有入力へ追加する。`GraphicsDeviceInit::present_fences`は同じpolicyからDevice要求bitを追加する。AHOではEngineConfigの値を両方へ展開し、Instance生成後に不足拡張を補おうとしない。

```cpp
aho::engine::StandardEngineConfig config;
config.engine.present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE;
aho::StandardEngine engine(config);
// 10aではmaintenance1を有効にしたDeviceまで。
// 実際のFence付きpresentと資源回収は10cで接続する。
```

headless／独自生成では、必要なInstance拡張を持つ親から`DeviceCreateInfo::features`へbitを要求できる。Device側が有効な依存を確認し、必要なDevice拡張を補う。

## backendの契約

Instanceは実際の拡張設定をmetadataへ保存する。queryはcore features2か、実際に有効なKHR features2を使う。timelineとmaintenance1は独立したchain要素であり、片方・両方・なしを扱う。拡張の列挙だけでfeature=trueと推定しない。

native生成時のchainも要求したbitだけを含め、呼出し中のscratchがpNext先を保持する。成功したnative Deviceにだけenabled bitを保存し、失敗時に要求を有効状態として公開しない。通常操作でqueryや所有graph探索を行わない。

maintenance1を有効にしただけでは、present operationへFenceを指定したことにならない。後続で`VkSwapchainPresentFenceInfoEXT`を実際のpresentへ接続し、そのsignalから外部PresentRecordの所有を回収する。device idleはその代わりの厳密証明として使わない。[present Fenceの保証](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html)

## 検証

新VSL7件とAHO1件を追加。native前の依存拒否、core／KHR query、timelineとの4形のnative chain、拡張名のみではdisabled、LEGACYではdisabled、AHOでInstance／Deviceを一つのpolicyから有効化する経路を確認した。

- 追加VSL7件・AHO起動14件、重点VSL132件／AHO47件、全VSL1,029件・AHO GPU158件／CPU1,070件、コンパイル契約195件が成功。
- Apple M1／MoltenVK1.4.2、validation＋ASan／UBSanを使用。新能力ケースのskipはなく、従来packed integer clearのdriver制約3件だけskip。LSanは無効。
- 規約13ファイル／7TUと最終test補足で違反0、source354件違反0。Impl寸法はInstance40／slot40／align8、Device56／slot80／align8で維持。
- install consumer25target、Debug／Release、x86_64 compile-only／archive、artifact混在link拒否、一時値借用の負例が成功。
- heap／arenaは3frame／12submission／2draw、resize1720×1080→2000×1360、Pipeline生成1回、PNG3枚が対応ごと一致。

他OS、KHRだけを提供するdriver、実driver OOM／device lostは未検証。性能率は未測定である。10aの能力検証は、実present Fenceのsignalや資源回収を検証したものではない。次は10bのSwapchain共通backend／退役状態へ進む。
