# StandardEngineの起動設定

起動時の値は`EngineConfig`、初期Windowの値は`InitialWindowConfig`に記述し、`StandardEngineConfig`にまとめて渡します。GPU資源を所有するEngineDataと設定は分離しています。

```cpp
aho::engine::StandardEngineConfig config;
config.engine.application_name = "application";
config.engine.descriptor_budget[vsl::graphic_resource::Type::UNIFORM_BUFFER] = 256;
config.initial_window = {.title = "main", .width = 1280, .height = 720};
config.register_as_default = false;

aho::StandardEngine engine(config, false);
engine.boot();
auto window = engine.get_window();
aho::Window second("second", 800, 600, engine);
```

`config()`から設定を参照できます。従来の`StandardEngine("name", boot_on)`と`boot("name")`も利用でき、初期Windowは既定400×400です。タイトルが空の場合はapplication_nameを使用します。`register_as_default=false`なら既定登録を変更せず、明示的なハンドルでWindowを利用できます。

物理デバイスは既定のVSL検索、または`physical_device_index`で列挙順の添字を選びます。`instance_extensions`は追加の拡張名です。Windowに必要な拡張はVSLが追加します。descriptor_budgetは標準descriptor種別ごとのpool予算で、OverAllocateManagerへ渡します。省略した種別の予算は0です。拡張固有のdescriptor設定はVSLを直接使用します。種別・ゼロ予算・合計の32bit超過はGPU資源の作成前に検証します。

bootは初期Window、デバイス資源、Window描画資源を順に作り、完了後に所有者と既定登録へ反映します。失敗時は仮Windowを閉じ、既存の登録を保持します。起動済みのbootは例外にし、共有ハンドルやWindowの参照先を途中で置き換えません。初期Windowと追加Windowの描画資源生成は同じ実装を使用します。

未使用だったEnginePrespecificationはEngineConfigへ置き換えました。shaderの検索ディレクトリはこのエンジンの起動処理では扱いません。
