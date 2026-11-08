# Descriptorの使用状態と要素単位の所有

[完了計画](arena-completion-plan.md)の順序5a。Layout／Pool／Setの配置を始める前に、通常VSLとTypedの状態・更新契約を統一する。

## 共有する状態

Setの物理使用状態は `GraphicResourceImpl::use` 一つとする。nativeの `BindGraphicResource`、Typedの `ResourceUseGroup::ForDescriptor`、bindを省略する `track()` が同じ状態へ登録する。複数phaseからの使用を許容し、すべての使用が完了／取消／失敗するまで更新とPoolの明示破棄を拒否する。

各bindingは配列要素ごとに次を持つ。

- Buffer／Image／Samplerの物理ownerと、そのownerに属するUseStateへの借用。
- AHO等が付加する論理制約のResourceUse。
- VSL経由で当該要素が初期化されたかどうか。

論理制約のownerを物理ownerの一覧へ重複保存しない。native更新は更新対象の要素の論理制約を外す。未更新要素のowner・制約・初期化状態は維持する。

Typed固有の初期化配列とdescriptor用Groupのbinding表を廃止した。`TypedDescriptorSet::require_complete()` はSetの表を参照するため、native更新も認識し、Pool無効化後は空layoutでも失敗する。未初期化の配列へnative scalar更新を行っても、要素0だけの初期化では完全にならない。

## 責務と実際の経路

通常の `Resource::update` とTypedの `detail::write_descriptor` は、公開入力を解決するadapterである。後者は配置接続までの移行入口として残す。

`operation::validate_descriptor_buffer/image` は解決済みImplのusage・型・range・alignment・device上限を検査する。`VK_WHOLE_SIZE` は実効rangeで判定する。native Image-onlyのGENERAL、Typed sampledのSHADER_READ_ONLY、native INPUT_ATTACHMENTは維持する。combined imageはsamplerを必要とする。

従来のImage＋Sampler overloadをSAMPLER bindingに使う呼び方も維持する。この場合、Vulkanが参照するSamplerだけを保持・使用登録し、無関係なImageのusageを要求しない。

`lifetime::write_descriptor_binding` はSetの有効性とidleを確認し、layout・配列範囲を検証する。物理owner／論理制約を含む全要素を準備し、mapの確保も済ませる。その後、`operation::write_descriptor` にnative DeviceとWriteDescriptorSetを渡し、成功後に表をswapする。native更新後のcommitは確保・例外を伴わない。途中失敗は既存要素を変更しない。

native scalar更新は配列要素0だけを置換する。nativeの既存明示type更新は、private layout metadataを持たない場合も維持する。型推論とTypedの完全性判定にはlayout metadataが必要である。

記録時はSet ownerを保持し、共通の `record_descriptor_resources` がSet・現在の物理資源・論理制約の使用を登録する。Typed用GroupとNativeBindが同じSetを二重走査しない。コマンド生成後・記録前の更新は記録へ反映する。COMPLETION保持とSUBMISSION保持の回収時点は[完了管理](completion-ownership.md)に従う。

## 完了callbackからの再入

使用状態のnative観測はsnapshotを使う。live一覧の削除は保存済みstatusだけで行い、native待機中に追加された使用が残れば `wait()` は成功扱いしない。新しいPENDING使用は観測前に登録し、観測失敗時も取消までは保守的に残す。観測によって現在phaseが取り消された場合、記録を拒否する。

Pool破棄は所属確認後にretiring guardを置き、Setのstrong snapshotを検査してからmanagerの一覧とnative poolを破棄する。callbackから同Poolへのallocate・record・write・nested destroyを拒否する。検査が失敗した場合、managerの管理とmetadataを維持する。

descriptor記録中もguardを置く。物理資源の完了callbackが現在phaseを取り消しても、記録処理が戻るまで要素表の変更とPool破棄を認めない。最後に取得したCompletionと現在の記録phaseが一致するか検査する。公開wrapper／Groupがcallbackで破棄されても、操作の局所ownerがSetを保持する。

これらは同一スレッドの再入への対策であり、複数CPUスレッド間の排他機構ではない。Vulkanのhost外部同期とVSLオブジェクトの並行操作は呼出側で管理する。

## 互換性と残る移行

通常のupdate／Typed write／bindの呼出形式は維持する。使用中のnative descriptor更新とPoolの明示destroyは、これまでより早く例外で拒否する。再更新は取消・完了を観測した後に行う。

直接の `vkUpdateDescriptorSets` はVSLの初期化表や資源保持を更新しない。既存のtexel buffer等のnative経路のため、NativeBind全体に完全初期化を強制しない。この入口は呼出側が初期化・所有・同期を管理する。

`ResourceUseGroup` は移行adapterとして残す。descriptor用 `replace` はlayoutのbinding順indexと要素数が一致する制約一覧を更新する。汎用Groupの独自状態・binding表は従来どおりである。

現段階のSet／Layout／Poolはheap上にある。SetにはUseStateを内包するが、要素表・owner一覧・使用一覧は可変長確保を残す。要素範囲の更新準備は当該bindingのmetadataを複製する。性能改善率は未測定である。

次の5bで生成／破棄backendと寸法・配置を接続し、5cでTyped schemaの正確な個数、寿命群、arena内部のborrow、可変の外部依存を扱う。配置前にこの共通状態の契約を維持する。

## 検証

新GPUテスト12件で、相互更新拒否、native部分更新後の未更新ownerとGPU出力、後半要素の不正入力による旧状態維持、論理制約置換、複数phase、Pool無効化、完了callbackの再入とwrapper破棄を確認した。

macOS arm64／Clang20／MoltenVK 1.4.2／validation／ASan・UBSanで、VSL772件中769成功・既知packed format3件skip、AHO GPU140件・CPU1070件、compile契約175件が成功。規約検査は変更C++16ファイル／11TUで違反0、最後のnative adapter修正も1TUで0。CMake272パス、MSBuildとfiltersの新4パスを確認した。

install consumerの公開APIだけでbuild／link／実行を確認した。Debug／Releaseの実寸法static assertと配置計画264 bytes／alignment8が一致し、同寸法でもDebugヘッダーとReleaseバイナリの混在をlink時に拒否した。両サンプルは3frame・12submission・2draw・238831更新画素、1720×1080→2000×1360のresize、pipeline生成1回、PNG3枚のbyte一致を確認した。検証ログにVUID／sanitizer errorはない。

LSanは無効。CPU並行、実DeviceLost、全native失敗位置への注入、Windows／Linux実機、性能率は未検証。driverの最終GPU allocation表示はリーク不在の証拠として扱わない。今回の寸法生成方式には変更を加えていない。
