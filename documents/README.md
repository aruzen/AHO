# 文書一覧

開発の入口とビルド手順は[ルートREADME](../README.md)に記載します。公開文書は`documents/`へまとめます。

- [C++標準・reflection・ツールチェーン](development/cxx-standard.md)
- [コーディング規約](development/style-decisions.md)と[規約検査](development/style-checks.md)
- [ビルドオプション](build-options.md)と[インストールpackage](installed-package.md)
- [Buffer](AHO/resource-buffer.md)・[Polygon](AHO/polygon.md)・[数学規約](AHO/math-conventions.md)
- [エンジンの所有権](AHO/engine-ownership.md)・[起動設定](AHO/engine-configuration.md)・[公開ヘッダー](AHO/public-headers.md)
- [experimental Mesh](experimental/mesh.md)・[Screenshot](AHO/experimental-screenshot.md)・[glTF](AHO/experimental-gltf.md)・[MSDF](msdf-text.md)
- [Typedの読み方](typed-readability.md)・[資源割当](typed-resource-allocation.md)・[フレーム資源](typed-frame-resources.md)・[使用例](typed-example.md)
- [TypedとAHOのBuffer連携](typed-buffer-interop.md)・[master統合](typed-master-integration.md)
- [RenderPassの構成](VSL/render-pass-composition.md)・[commandの資源寿命](VSL/command-resource-lifetime.md)・[画像同期](VSL/image-shader-visibility.md)
- [VSLの所有・配置再設計](VSL/ownership-and-placement.md)・[移行計画](VSL/ownership-migration-plan.md)・[Buffer配置の実装と使用例](VSL/buffer-placement.md)
- [Bufferの世代・読み取り・資源ID・寿命群](VSL/buffer-generation-contracts.md)
- [Bufferの4層配置・借用入口・owner保持](VSL/buffer-layer-layout.md)
- [4責務とarenaの段階的移行・Typed bundle配置](VSL/arena-migration.md)
- [Device／Instanceまで含むarena化の完了計画](VSL/arena-completion-plan.md)
- [異種Implの寸法生成とResourceIdからのCPU配置計画](VSL/resource-placement-plan.md)
- [Buffer／Image／Samplerを同じ領域へ構築するResourceArena](VSL/resource-arena.md)
- [Typed schema／bundleによるImage／Samplerの配置とcomponent共有](VSL/typed-owned-resources.md)
- [AHO Bufferのsnapshot・固定import・可変arena世代](AHO/arena-buffer.md)
- [Image／Samplerの共通backend・所有・使用状態](VSL/image-sampler-backend.md)
