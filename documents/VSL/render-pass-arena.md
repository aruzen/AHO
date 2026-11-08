# RenderPassの共通backendとCPU配置

[完了計画](arena-completion-plan.md)の7a。通常のRenderPassとResourceArenaから、同じoperation／lifetime処理を利用する。Pipeline／Framebufferの配置は7b／7cに続ける。

## 責務と構築

`operation/render_pass_description.hpp`に生成入力とclearの値型を分離した。既存の`vulkan/render_pass.hpp`からもincludeするため、通常の生成引数と型名は維持する。descriptionの形状検証とnative生成はoperationの一経路へ集約し、Deviceのcolor出力数制限とnative配列countの範囲も生成前に検査する。

lifetimeは指定slotへRenderPassImplを構築し、失敗時にCPU実体と生成済nativeを回収する。heap ownerもこの処理を使う。arenaは成功済slotを逆順に回収し、失敗後は同じArenaMemoryから再構築できる。operationのdestroyが保存済native Deviceから`destroyRenderPass`を実行し、lifetimeがその時点とCPU実体の破棄を管理する。

Implにはnative Device／RenderPass、Deviceへの借用、owner nodeと`optional<const RenderPassDescription>`を置く。不変descriptionはFramebuffer検証とRenderPassBeginのclear生成にも必要なruntime値であり、recipe用owner層を追加しない。descriptionの別shared_ptr確保を除去し、値本体をImpl内へ配置した。vector等の可変長領域とnative内部のメモリは一括CPU領域に含めない。

RenderPassに独立したGPU UseStateは付けない。attachmentには既存Imageの物理状態を使い、GPU完了までのRenderPass保持はFramebuffer／記録Workからの所有で保証する。heap owner／arena rootがDeviceを保持し、Implはarena自身を所有しない。

```cpp
// 通常VSLも同じbackendを利用する。
vsl::RenderPass pass(device, description);

namespace t = vsl::typed;
namespace l = vsl::lifetime;
constexpr std::array<t::ResourceRequest, 2> requests{{
    {{10}, vsl::storage::ResourceKind::RENDER_PASS},
    {{20}, vsl::storage::ResourceKind::RENDER_PASS}}};
constexpr t::ResourcePlan<2> plan(requests);
const std::array<l::ResourceInit, 2> inputs{description, description};
l::ResourceArena arena(device, plan, inputs);
auto first = arena.render_pass({10});
auto second = arena.render_pass({20});
vsl::FrameBuffer target(first, width, height, {color});
```

各IDから異なるslotのImplへ直接到達する。owned Accessorは正規arena ownerへaliasし、externalには元のAccessorを渡す。importのgetterは元ownerを返し、importerの無関係なslotを保持しない。RenderPass importも固定親として依存graphへ登録するため、descriptorの逆向き更新で既知rootの循環を作れない。

`description()`の参照はlvalueのAccessorが所有を続ける間の借用とする。constを含む一時Accessorからの取得はコンパイル時に拒否する。保存する場合は値をコピーする。`get_device()`は実Deviceの正規ownerを返し、子RenderPassを保持するaliasを返さない。shared_from_thisと公開data_は順序9／12の移行用入口として残る。

## 記録・完了・取消

arena → owning RenderPassAccessor → owning FrameBuffer → RenderPassBegin／Typed draw → 記録Workの順で保持する。元arena／pipeline facadeを破棄しても保存commandを記録できる。記録処理は保持済Implとnative handleを使い、依存graphを探索しない。

Completionは結果だけを保存する。現在のSUBMISSION保持は、GPU完了後にCommandManagerのslotでWorkが置換されるまで続く。取消時はnative記録をresetし、PhaseのWork所有がなくなった後に回収する。これらの既存契約を変更せず実GPUで確認した。Framebufferの内部強い所有は7cへ、Work registryの配置は8へ残す。

## 寸法と検証

ResourceKind::RENDER_PASSは生成されたImplLayoutを使い、USE_OFFSETはNO_SEPARATE_USEとする。private実定義とslotのstatic_assert、compile-only probe、ABI fingerprint、構成別header、installへ接続した。description／公開facadeを生成入力のhashに含め、異なるartifactのheader／binary混在をlink guardで拒否する。

CPU3件・GPU6件を追加した。同型slotとinline description、元入力変更からの独立、元arena破棄後の利用、canonical Device getterとimport owner、別Device／空／非所有importの拒否、先行native生成後の失敗と同じ領域の再利用、GPU完了／取消後の回収、固定親の循環拒否を検証する。一時descriptionの拒否と各public headerの単独includeにはcompile4件を追加した。

macOS arm64／Clang20／C++26／ASan・UBSan／MoltenVK・validationで、VSL849件中846成功・既知packed format3skip、AHO GPU150件／CPU1070件、compile契約185件が成功した。heap／arenaサンプルはいずれも3frame・12submission・2draw、238831更新画素、resize 1720×1080→2000×1360、pipeline生成1回を維持し、PNG3枚が一致した。VUID／sanitizer errorはない。

installed public consumer12targetのbuild／link／CPU実行が成功した。private Implとbackend headerはinstallしない。このarm64構成のRenderPassImplは112 byte・alignment 8で、9種を一つずつ置く計画は864 byte・alignment 8となる。GPUメモリ量や性能改善率を示す値ではない。

Debug／Releaseの実寸法assertとCPU利用、x86_64向けcross archive、異artifactのlink拒否、installed headerでの一時description借用拒否が成功した。変更C++20ファイル／14TUのstrict違反0、CMake297パスの検査も成功した。独立レビューを反映し、空descriptionをclear生成で参照しないことを確認した。

native driver／allocator由来の全失敗位置の注入、LSan、Windows／Linux実機、CPU並行、実DeviceLost、性能率は未検証。Deviceはまだheapであり、FramebufferやPipelineには後続の配置移行が残る。
