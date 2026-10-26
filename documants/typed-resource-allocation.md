# Typed資源の割当宣言と生成

## 発見した不足

TypedFrameResourcesは任意の型付き資源構造体を管理するが、bufferの生成・descriptorの生成・writeによる接続は利用側のconstructor／Factoryに残っている。ResourceSchemaだけでは、buffer内部の要素数・フレーム枠ごとの独立性・外部資源との共有方法は確定できない。

既存のResourceSchemaをshaderとの契約として維持し、別の割当schemaでこれらを宣言する。stackは割当schemaと計画、資源の自動生成と接続、Sample適用の3段階とする。

## 割当schema

- `ResourceAllocationSchema<Resources, Allocations...>`はshaderの全bindingをちょうど1回ずつ指定する。不足・重複・未知のbinding・画像へのbuffer割当・不正なscopeはcompile時に拒否する。
- `BufferAllocation<set, binding, FixedElements<N>, ResourceScope::PER_FRAME>`で各descriptor要素につきNレコードのbufferを作る。既定は1要素、PER_FRAME。
- `ResourceScope::SHARED`はbufferを全枠で共有する。同じsetにPER_FRAMEのbindingがあればsetは枠ごと、全bindingが共有ならsetも1つを共有する。
- `RuntimeElements`は`ResourceAllocationSizes<Schema>::set<set, binding>(count)`で実行時の要素数を指定する。未指定・0は計画時に拒否する。
- `ExternalAllocation<set, binding>`は既存のTyped資源を接続する。画像サイズや初期layoutなどは既存の資源作成・同期APIで指定する。
- descriptorの配列数はResourceSchema、buffer内部の要素数は割当schemaが持つ。配列の各bufferは同じ要素数で、別々に生成する。

`ResourceAllocationPlan<Schema>`はGPUを使わずにbuffer数、descriptor set／descriptor数、論理payload byte数を計算する。固定容量ならconstexprでも利用できる。積と合計のoverflowは検出する。

## メモリ計画の境界

payload byte数はC++のsizeofと要素数の積。Vulkan memory requirements、alignment、専用allocation、descriptor poolのdriver内部メモリ、ホスト側pimplの確保量は含めない。今回は既存の所有方式で生成し、arena・placement newは後続の段階とする。

## 検証

- 割当計画: LLVM 20 / C++26 / DebugでVSLとテストをビルド。CPUテスト5件・コンパイル契約11件成功。変更C++ 3ファイル／2TUのformat・命名・配置検査は違反0件。
- Debugビルド中に環境の空き容量不足が発生した。並列数を6から2へ下げて再開し、ビルド完了。製品コードや他の作業ファイルの削除は不要だった。
