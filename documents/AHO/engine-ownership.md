# エンジンの所有権と既定登録

`GraphicalEngine`・`StandardEngine`は起動と描画Windowを管理する所有者で、コピー・ムーブしません。共有するときは`EngineAccessor handle = engine;`としてハンドルを取得します。ハンドルのコピーはEngineDataを共有し、ムーブはその所有権を移します。ハンドルやWindow・RenderContextが残っている間はGPU資源も保持されます。

EngineAccessorを構築しただけでは、既定エンジンを登録しません。`DefaultEngineRegistration registration(handle);`の寿命が既定登録の有効期間です。最新の有効な登録を`Current()`から値で取得し、登録が終了すると以前の有効な登録へ戻ります。古い登録を先に破棄した場合や、登録オブジェクトをムーブした場合も有効です。登録内部の生ポインタは公開しません。

StandardEngineは起動に成功した後で自身を登録し、破棄時に登録を解除します。未起動のStandardEngineが、別の起動済みエンジンの既定登録を置き換えることはありません。既定登録の取得・追加は同期しますが、GPUやWindowの操作を任意スレッドから行えるという保証ではありません。

```cpp
aho::StandardEngine owner("application");
aho::engine::EngineAccessor handle = owner;
aho::Window second("second", 800, 600, handle);
```

従来の`engine.data_`は`engine.data()`へ、`EngineAccessor::Instance()`は`DefaultEngineRegistration::Current()`へ移行してください。`data()`はshared_ptrのconst参照を返し、ハンドル内部のポインタを利用側から差し替えることはできません。専門的な制御向けにEngineData内のVSL資源は引き続き扱えます。独自のEngineDataを使う場合は明示的なEngineAccessorのコンストラクタに渡します。
