//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_BUFFER_AND_IMAGE_ACCESSOR_H
#define AHO_ALL_BUFFER_AND_IMAGE_ACCESSOR_H

#include "../define.hpp"
#include "pv.hpp"

#include "../format.hpp"
#include "../utils/flags.hpp"

#include "command.hpp"

namespace VSL_NAMESPACE {
    enum class MemoryType : unsigned int {
        None = 0,
        // データ転送の「送信元」バッファ。他のバッファや画像へデータをコピーするために使用。
        // 例: CPU から GPU にデータを送るための Staging Buffer。
        TransferSource = 0x00000001,

        // データ転送の「受信先」バッファ。他のバッファや画像からデータを受け取るために使用。
        // 例: テクスチャや頂点バッファへデータを転送する際に利用。
        TransferDestination = 0x00000002,

        // シェーダーがテクスチャのように読み込めるが、変更されないバッファ。
        // 例: ルックアップテーブルや静的なデータの格納。
        UniformTexelBuffer = 0x00000004,

        // シェーダーが自由に読み書きできるバッファ。
        // 例: 動的な画像データや GPGPU 処理用のバッファ。
        StorageTexelBuffer = 0x00000008,

        // シェーダーが参照する「読み取り専用」のデータバッファ。
        // 例: マテリアルの設定値や変換行列 (MVP マトリクス) などを格納。
        UniformBuffer = 0x00000010,

        // シェーダーが自由に読み書きできるデータバッファ。
        // 例: 計算シェーダーで使用するバッファ (SSBO)。
        StorageBuffer = 0x00000020,

        // 描画時に頂点の描画順序を指定するためのバッファ。
        // 例: `vkCmdDrawIndexed()` で使用するインデックスデータ。
        IndexBuffer = 0x00000040,

        // 頂点の座標や色、テクスチャ座標などを格納するバッファ。
        // 例: `vkCmdBindVertexBuffers()` でバインドされる。
        VertexBuffer = 0x00000080,

        // GPU が間接描画 (`vkCmdDrawIndirect`) を行うためのバッファ。
        // 例: GPU の計算結果をもとに描画命令を決定する場合に使用。
        IndirectDrawBuffer = 0x00000100,

        // シェーダーから GPU メモリのアドレスを直接取得できるバッファ。
        // 例: レイトレーシングや先進的な GPU メモリアクセス技術で使用。
        ShaderDeviceAddress = 0x00020000,

        // ハードウェアによる動画デコードの入力データを格納するバッファ。
        // 例: 圧縮された動画データを GPU でデコードする際に使用。
        VideoDecodeSource = 0x00002000,

        // ハードウェアによる動画デコードの出力データを格納するバッファ。
        // 例: デコード済みの映像フレームを保存し、レンダリングする場合に使用。
        VideoDecodeDestination = 0x00004000,

        // 頂点シェーダーの出力を保存するためのバッファ。
        // 例: 変換後の頂点データを後で利用する場合に使用。
        TransformFeedbackBuffer = 0x00000800,

        // トランスフォームフィードバック時に記録されるカウンター情報を保存するバッファ。
        // 例: シェーダーで生成された頂点数を記録する場合に使用。
        TransformFeedbackCounter = 0x00001000,

        // GPU が条件に応じて描画をスキップできるようにするバッファ。
        // 例: `vkCmdBeginConditionalRenderingEXT()` でレンダリングの有無を制御する。
        ConditionalRendering = 0x00000200,

        // レイトレーシングの衝突判定 (BVH) を構築するための入力データ。
        // 例: 三角形の頂点情報やオブジェクトのバウンディングボックスデータ。
        AccelerationStructureBuildInput = 0x00080000,

        // レイトレーシング用の空間構造 (BVH) を格納するバッファ。
        // 例: `vkCmdBuildAccelerationStructuresKHR()` で構築した BVH を保存。
        AccelerationStructureStorage = 0x00100000,

        // レイトレーシングシェーダーを管理するための特別なバッファ。
        // 例: `vkCmdTraceRaysKHR()` で使用されるシェーダーバインディングテーブル (SBT)。
        ShaderBindingTable = 0x00000400,

        // ハードウェアによる動画エンコードの出力データを格納するバッファ。
        // 例: GPU でエンコードされた圧縮映像フレームを保存。
        VideoEncodeDestination = 0x00008000,

        // ハードウェアによる動画エンコードの入力データを格納するバッファ。
        // 例: エンコード前の映像フレームを格納し、GPU でエンコードする。
        VideoEncodeSource = 0x00010000,

        // サンプラーの情報 (テクスチャのフィルタリング設定など) を格納するバッファ。
        // 例: `VK_DESCRIPTOR_TYPE_SAMPLER` をバッファとして管理する場合に使用。
        SamplerDescriptorBuffer = 0x00200000,

        // リソース (テクスチャやバッファ) の情報を格納するデスクリプタバッファ。
        // 例: `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` などをバッファで管理する場合に使用。
        ResourceDescriptorBuffer = 0x00400000,

        // プッシュデスクリプタの情報を格納するバッファ。
        // 例: `VK_DESCRIPTOR_TYPE_PUSH_DESCRIPTOR_KHR` を使用する場合に必要。
        PushDescriptorBuffer = 0x04000000,

        // マイクロマップ (微細なジオメトリデータ) を構築するための入力データを格納するバッファ。
        // 例: `vkBuildMicromapsEXT()` で使用されるデータを格納。
        MicromapBuildInput = 0x00800000,

        // マイクロマップデータ (ジオメトリの詳細情報) を格納するバッファ。
        // 例: `vkCmdBuildMicromapsEXT()` で構築されたデータを保存。
        MicromapStorage = 0x01000000,
    };

    enum class ImageType {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        ColorAttachment = 0x00000010,
        DepthStencilAttachment = 0x00000020,
        TransientAttachment = 0x00000040,
        InputAttachment = 0x00000080,
        HostTransfer = 0x00400000,
        VideoDecodeDst = 0x00000400,
        VideoDecodeSrc = 0x00000800,
        VideoDecodeDpb = 0x00001000,
        FragmentDensityMap = 0x00000200,
        FragmentShadingRateAttachment = 0x00000100,
        VideoEncodeDst = 0x00002000,
        VideoEncodeSrc = 0x00004000,
        VideoEncodeDpb = 0x00008000,
        AttachmentFeedbackLoop = 0x00080000,
        VideoEncodeQuantizationDeltaMap = 0x02000000,
        VideoEncodeEmphasisMap = 0x04000000,
    };

    enum class MemoryProperty {
        None = 0,
        // GPU 専用メモリ: メモリが GPU に最適化され、CPU からはアクセスできない。
        // 例: 高速な VRAM (GPU のメインメモリ) に割り当てられるメモリ。
        DeviceLocal = 0x00000001,

        // CPU からアクセス可能なメモリ: CPU から直接読み書きできるメモリ。
        // 例: `Staging Buffer` やデータ転送用バッファ。
        HostVisible = 0x00000002,

        // メモリの同期が保証される: CPU と GPU の間でデータが即座に同期される。
        // 例: CPU から書き込んだデータをすぐに GPU が利用可能にする。
        HostCoherent = 0x00000004,

        // CPU 側でキャッシュされるメモリ: CPU のキャッシュが有効で、読み取り速度が速くなる。
        // 例: 頻繁に CPU から読み取るデータに適している。
        HostCached = 0x00000008,

        // 遅延割り当てメモリ: メモリが必要になるまで実際に割り当てられない。
        // 例: メモリ使用を節約するため、使われるまで割り当てを延期。
        LazilyAllocated = 0x00000010,

        // 保護されたメモリ: セキュアなデータが保存される、変更できないメモリ。
        // 例: 安全性の高いアプリケーションに使用される。
        Protected = 0x00000020,

        // AMD 用のデバイスコヒーレントメモリ: AMD GPU 用に、CPU と GPU 間で即座に同期されるメモリ。
        // 例: AMD ハードウェアで動作するアプリケーションに使用。
        DeviceCoherentAmd = 0x00000040,

        // AMD 用のデバイス非キャッシュメモリ: GPU がキャッシュしないメモリ。
        // 例: 高速化が不要な一部のデータに使用。
        DeviceUncachedAmd = 0x00000080,

        // NVIDIA RDMA 対応メモリ: ネットワーク経由で直接メモリにアクセス可能なメモリ。
        // 例: RDMA (Remote Direct Memory Access) 対応のアプリケーションに使用。
        RdmaCapableNv = 0x00000100,
    };

    enum class ImageLayout {
        Undefined = 0,
        General = 1,
        ColorAttachmentOptimal = 2,
        DepthStencilAttachmentOptimal = 3,
        DepthStencilReadOnlyOptimal = 4,
        ShaderReadOnlyOptimal = 5,
        TransferSrcOptimal = 6,
        TransferDstOptimal = 7,
        Preinitialized = 8,
    };

    enum class SharingMode {
        Exclusive = 0,
        Concurrent = 1,
    };

    ___VSL_EXPAND_FLAGS_OPERATOR(ImageType);

    ___VSL_EXPAND_FLAGS_OPERATOR(ImageLayout);

    ___VSL_EXPAND_FLAGS_OPERATOR(MemoryProperty);

    ___VSL_EXPAND_FLAGS_OPERATOR(MemoryType);

    struct ImageAccessor;

    struct BufferAccessor {
        struct LocalBufferHolder {
            BufferAccessor *parent;
            size_t size, offset;
            void *data;

            ~LocalBufferHolder();

            void flush();

            template<typename... Args>
            bool copy(const Args &... args);

            template<typename... Args>
            void uncheck_copy(const Args &... args);

            bool copyByBuffer(CommandManager commandManager, BufferAccessor *buf);

            bool copyByImage(CommandManager commandManager, const ImageAccessor &image);
        };

        std::shared_ptr<_impl::Buffer_impl> _data;


        virtual LocalBufferHolder data(std::optional<size_t> size = std::nullopt, size_t offset = 0) = 0;

        virtual void flush(LocalBufferHolder holder) = 0;

        template<typename... Args>
        bool copy(const Args &... args);

        template<typename... Args>
        void uncheck_copy(const Args &... args);

        bool copyByBuffer(CommandManager commandManager, BufferAccessor *buf);

        bool copyByImage(CommandManager commandManager, const ImageAccessor &image);

        size_t size();

        static std::shared_ptr<_impl::Buffer_impl>
        MakeBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, MemoryType memType,
                   MemoryProperty memProperty, SharingMode sharingMode);

        static void
        FlushBuffer(LocalBufferHolder holder, MemoryType memType, MemoryProperty memProperty, SharingMode sharingMode);

        static LocalBufferHolder
        GetData(VSL_NAMESPACE::BufferAccessor *data, std::optional<size_t> size, size_t offset, MemoryType memType,
                MemoryProperty memProperty, SharingMode sharingMode);
    };

    struct ImageAccessor {
        std::shared_ptr<_impl::Image_impl> _data;

        bool copyByBuffer(CommandManager commandManager, BufferAccessor * buf, vsl::ImageLayout layout);

//        bool copyByImage(CommandManager commandManager, const ImageAccessor& image, std::optional<vsl::FenceHolder> waitFence = std::nullopt);

        size_t size();

        static std::shared_ptr<_impl::Image_impl> MakeImage(vsl::ImageType type,
                                                            vsl::MemoryProperty prop,
                                                            vsl::ImageLayout layout,
                                                            vsl::LogicalDeviceAccessor device,
                                                            std::uint32_t width,
                                                            std::uint32_t height, data_format::___Format format);

        static void TransformLayout(CommandManager manager,
                                    std::shared_ptr<_impl::Image_impl> data,
                                    vsl::ImageLayout oldLayout,
                                    vsl::ImageLayout newLayout);
    };
}
#endif //AHO_ALL_BUFFER_AND_IMAGE_ACCESSOR_H
