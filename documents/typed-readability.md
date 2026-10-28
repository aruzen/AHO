# Typed実装の読み方

6段のTyped拡張で頂点bufferの実体とpipeline、descriptorのschemaとallocationが同じヘッダーへ集まっていた。今回の整理では責務ごとに移動し、batchのstate比較を対象別の処理へ分けた。公開API・型制約・診断・所有権・記録順序は維持する。

## ファイルの役割

| ファイル | 読む対象 |
| --- | --- |
| [typed_vertex_layout.hpp](../VSLi/VSL/include/VSL/utils/typed_vertex_layout.hpp) | C++の頂点record、attribute、bindingの宣言 |
| [typed_block_layout.hpp](../VSLi/VSL/include/VSL/utils/typed_block_layout.hpp) | UBO/SSBO/pushのC++メモリ配置の宣言 |
| [typed_descriptor_schema.hpp](../VSLi/VSL/include/VSL/utils/typed_descriptor_schema.hpp) | descriptorのbinding/layout、set番号、ResourceSchema |
| [typed_push_constants.hpp](../VSLi/VSL/include/VSL/utils/typed_push_constants.hpp) | push range/schema、値のsnapshot |
| [typed_vertex_buffer.hpp](../VSLi/VSL/include/VSL/utils/typed_vertex_buffer.hpp) | pipelineから独立した頂点buffer/viewと型の適合判定 |
| [typed_resources.hpp](../VSLi/VSL/include/VSL/utils/typed_resources.hpp) | UBO/SSBO/image/samplerの実体、descriptor allocation/update |
| [typed_pipeline.hpp](../VSLi/VSL/include/VSL/utils/typed_pipeline.hpp) / [typed_compute.hpp](../VSLi/VSL/include/VSL/utils/typed_compute.hpp) | schemaの検証、binding、draw/dispatchの構築と記録 |
| [typed_batch.hpp](../VSLi/VSL/include/VSL/utils/typed_batch.hpp) | bind省略計画、所有するcommand群の記録 |
| [typed_shader.cpp](../VSLi/VSL/src/utils/typed_shader.cpp) / [typed_block_validation.cpp](../VSLi/VSL/src/utils/typed_block_validation.cpp) | shader reflectionとC++宣言の照合 |

`typed_pipeline.hpp`から頂点buffer、`typed_resources.hpp`からdescriptor schemaも引き続きincludeされる。既存のincludeはそのまま使え、宣言やbufferだけを読むときは分離したヘッダーを開ける。

## 実行までの流れ

1. layout/schemaで型・stage・binding・offset等を宣言する。
2. buffer、descriptor set、pipelineを作る。pipeline構築時にshaderと宣言を照合する。
3. bindingへvertex/resource/pushを揃え、draw/dispatch commandを作る。
4. command自身、またはTypedBatchから記録する。native commandがGPU完了までownerを保持する。

batchは`build_plans` → `plan_one` → `BatchState::update_*`の順に読む。`update_*`は比較用stateを更新し、bindが必要ならtrueを返す。`plan_one`はその結果をplanと統計へ反映する。pipeline変更時にdescriptorの比較状態を捨てる理由は`update_pipeline`に集めた。記録側は`invoke_all` → `invoke_one`の順にplanを適用し、各push snapshotも反映する。

長いBlockType初期化子をfieldごとに改行し、pushのCPU収納offsetには`data_offset`、重複判定には`same_stage` / `overlaps`という名前を付けた。確保方式や機能追加は別課題とし、詳細な制約は[typed-stack.md](typed-stack.md)に記録している。

## 検証

Apple M1/macOS、LLVM 20/C++23/Debug、MoltenVK 1.4.2 + validation layerで関連CTest **290/290成功、skipなし**。Typed compile contract 73件と、配置検証・descriptor/push・draw/indexed draw・view/subpass・batch等の既存CPU/GPUテストを実行した。unexpected validation errorなし。

製品VSL・vsl-test・7種類の正例contract objectをビルドし、分離した2ヘッダーの単独includeも確認した。MSBuild XMLとdiff検査も成功。Windows/Linux、sanitizer、全AHO/VSLテストは未実施。

変更ヘッダー7件を含む10ファイルを既存3TUで命名・配置・format検査し、違反0件。
