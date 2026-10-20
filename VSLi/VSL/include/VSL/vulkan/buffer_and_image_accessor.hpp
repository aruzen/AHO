#ifndef AHO_ALL_BUFFER_AND_IMAGE_ACCESSOR_H
#define AHO_ALL_BUFFER_AND_IMAGE_ACCESSOR_H

#include "../define.hpp"
#include "pv.hpp"

#include "../format.hpp"
#include "../utils/flags.hpp"

#include "device.hpp"
#include <memory>
#include <concepts>
#include <type_traits>
#include <optional>
#include <span>
#include <cstddef>

namespace VSL_NAMESPACE {
    struct CommandManager;

    enum class MemoryType : unsigned int {
        None = 0,
        // データ転送の「送信元」バッファ。他のバッファや画像へデータをコピーするために使用。
        TransferSource = 0x00000001,

        // データ転送の「受信先」バッファ。他のバッファや画像からデータを受け取るために使用。
        TransferDestination = 0x00000002,

        // シェーダーがテクスチャのように読み込めるが、変更されないバッファ。
        UniformTexelBuffer = 0x00000004,

        // シェーダーが自由に読み書きできるバッファ。
        StorageTexelBuffer = 0x00000008,

        // シェーダーが参照する「読み取り専用」のデータバッファ。
        UniformBuffer = 0x00000010,

        // シェーダーが自由に読み書きできるデータバッファ。
        StorageBuffer = 0x00000020,

        // 描画時に頂点の描画順序を指定するためのバッファ。
        IndexBuffer = 0x00000040,

        // 頂点の座標や色、テクスチャ座標などを格納するバッファ。
        VertexBuffer = 0x00000080,

        // GPU が間接描画 (`vkCmdDrawIndirect`) を行うためのバッファ。
        IndirectDrawBuffer = 0x00000100,

        // シェーダーから GPU メモリのアドレスを直接取得できるバッファ。
        ShaderDeviceAddress = 0x00020000,

        // ハードウェアによる動画デコードの入力データを格納するバッファ。
        VideoDecodeSource = 0x00002000,

        // ハードウェアによる動画デコードの出力データを格納するバッファ。
        VideoDecodeDestination = 0x00004000,

        // 頂点シェーダーの出力を保存するためのバッファ。
        TransformFeedbackBuffer = 0x00000800,

        // トランスフォームフィードバック時に記録されるカウンター情報を保存するバッファ。
        TransformFeedbackCounter = 0x00001000,

        // GPU が条件に応じて描画をスキップできるようにするバッファ。
        ConditionalRendering = 0x00000200,

        // レイトレーシングの衝突判定 (BVH) を構築するための入力データ。
        AccelerationStructureBuildInput = 0x00080000,

        // レイトレーシング用の空間構造 (BVH) を格納するバッファ。
        AccelerationStructureStorage = 0x00100000,

        // レイトレーシングシェーダーを管理するための特別なバッファ。
        ShaderBindingTable = 0x00000400,

        // ハードウェアによる動画エンコードの出力データを格納するバッファ。
        VideoEncodeDestination = 0x00008000,

        // ハードウェアによる動画エンコードの入力データを格納するバッファ。
        VideoEncodeSource = 0x00010000,

        // サンプラーの情報 (テクスチャのフィルタリング設定など) を格納するバッファ。
        SamplerDescriptorBuffer = 0x00200000,

        // リソース (テクスチャやバッファ) の情報を格納するデスクリプタバッファ。
        ResourceDescriptorBuffer = 0x00400000,

        // プッシュデスクリプタの情報を格納するバッファ。
        PushDescriptorBuffer = 0x04000000,

        // マイクロマップ (微細なジオメトリデータ) を構築するための入力データを格納するバッファ。
        MicromapBuildInput = 0x00800000,

        // マイクロマップデータ (ジオメトリの詳細情報) を格納するバッファ。
        MicromapStorage = 0x01000000,
    };

    enum class SampleCount : std::uint32_t {
        One = 1,
        Two = 2,
        Four = 4,
        Eight = 8,
        Sixteen = 16,
        ThirtyTwo = 32,
        SixtyFour = 64
    };

    enum class ImageType {
        None = 0,
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
        // GPUからのアクセスに適したメモリ。CPUからアクセスできるかはHostVisibleによる。
        DeviceLocal = 0x00000001,

        // CPU からアクセス可能なメモリ: CPU から直接読み書きできるメモリ。
        HostVisible = 0x00000002,

        // CPUキャッシュの明示的なフラッシュ・無効化が不要。GPUの実行完了待ちは別途必要。
        HostCoherent = 0x00000004,

        // CPU 側でキャッシュされるメモリ: CPU のキャッシュが有効で、読み取り速度が速くなる。
        HostCached = 0x00000008,

        // 遅延割り当てメモリ: メモリが必要になるまで実際に割り当てられない。
        LazilyAllocated = 0x00000010,

        // 保護されたGPU処理で使用するメモリ。CPUからはアクセスできない。
        Protected = 0x00000020,

        // AMD拡張による、デバイス側キャッシュの明示的な可視性操作が不要なメモリ。
        DeviceCoherentAmd = 0x00000040,

        // AMD 用のデバイス非キャッシュメモリ: GPU がキャッシュしないメモリ。
        DeviceUncachedAmd = 0x00000080,

        // NVIDIA RDMA 対応メモリ: ネットワーク経由で直接メモリにアクセス可能なメモリ。
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
        Present = 1000001002,
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
        BufferAccessor() = default;
        BufferAccessor(const BufferAccessor&) = default;
        BufferAccessor& operator=(const BufferAccessor&) = default;
        BufferAccessor(BufferAccessor&&) = default;
        BufferAccessor& operator=(BufferAccessor&&) = default;
        virtual ~BufferAccessor() = default;

        // マップはストレージと共有する。holderの破棄ではunmapしない。
        struct LocalBufferHolder {
            // 元のハンドルを指す互換用の非所有参照。内部の寿命管理には使用しない。
            BufferAccessor* parent;
            std::size_t size, offset;
            void* data;

            LocalBufferHolder(const LocalBufferHolder&) = delete;

            LocalBufferHolder& operator=(const LocalBufferHolder&) = delete;

            LocalBufferHolder(LocalBufferHolder&& other);

            LocalBufferHolder(BufferAccessor* parent, std::size_t size, std::size_t offset, void* data = nullptr);

            ~LocalBufferHolder();

            void flush();
            void flush(std::size_t bytes, std::size_t offset = 0) const;
            void invalidate(std::size_t bytes, std::size_t offset = 0) const;
            // GPUの使用完了後に呼ぶ。範囲はこのholderの先頭から指定する。
            std::span<const std::byte> read(std::size_t bytes, std::size_t offset = 0) const;
            void write(std::span<const std::byte> bytes, std::size_t offset = 0) const;

            template<typename... Args>
            bool copy(const Args&... args);

            template<typename T>
            bool copy(T* t, std::size_t size);

            template<typename T, typename Count>
            requires(!std::same_as<Count, std::size_t> &&
                     (std::integral<Count> || (std::is_enum_v<Count> && std::is_convertible_v<Count, std::size_t>)))
            bool copy(T* t, Count size);

            template<typename... Args>
            void uncheck_copy(const Args&... args);

            bool copyByBuffer(CommandManager commandManager, BufferAccessor* buf);

            bool copyByImage(CommandManager commandManager, const ImageAccessor& image);

        private:
            std::shared_ptr<_impl::Buffer_impl> owner_;
            void synchronize(std::size_t bytes, std::size_t offset, bool flush) const;
        };

        std::shared_ptr<_impl::Buffer_impl> _data;

        virtual LocalBufferHolder data(std::optional<size_t> size = std::nullopt, size_t offset = 0) = 0;

        virtual void flush(LocalBufferHolder& holder) = 0;

        template<typename... Args>
        bool copy(const Args&... args);

        template<typename T>
        bool copy(T* t, size_t size);

        template<typename T, typename Count>
        requires(!std::same_as<Count, std::size_t> &&
                 (std::integral<Count> || (std::is_enum_v<Count> && std::is_convertible_v<Count, std::size_t>)))
        bool copy(T* t, Count size);

        template<typename... Args>
        void uncheck_copy(const Args&... args);

        bool copyByBuffer(CommandManager commandManager, BufferAccessor* buf);

        bool copyByImage(CommandManager commandManager, const ImageAccessor& image);

        size_t size();
        // 要求したバイト数を返す。
        std::size_t requestedSize() const;
        LogicalDeviceAccessor getDevice() const;
        MemoryType getUsage() const;
        MemoryProperty getMemoryProperties() const;

        static std::shared_ptr<_impl::Buffer_impl>
        MakeBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, MemoryType memType,
                   MemoryProperty memProperty, SharingMode sharingMode);

        static void
        FlushBuffer(LocalBufferHolder& holder, MemoryType memType, MemoryProperty memProperty, SharingMode sharingMode);

        static LocalBufferHolder
        GetData(VSL_NAMESPACE::BufferAccessor* data, std::optional<size_t> size, size_t offset, MemoryType memType,
                MemoryProperty memProperty, SharingMode sharingMode);

    private:
        void flushCopied(LocalBufferHolder& holder, std::size_t bytes);
    };

    struct ImageAccessor {
        std::shared_ptr<_impl::Image_impl> _data;
        data_format::___Format getFormat() const;
        ImageType getUsage() const;
        LogicalDeviceAccessor getDevice() const;

        bool copyByBuffer(CommandManager commandManager, BufferAccessor* buf, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx);

        size_t size();

        static std::shared_ptr<_impl::Image_impl> MakeImage(VSL_NAMESPACE::ImageType type,
                                                            VSL_NAMESPACE::MemoryProperty prop,
                                                            VSL_NAMESPACE::ImageLayout layout,
                                                            VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                            std::uint32_t width,
                                                            std::uint32_t height,
                                                            data_format::___Format format,
                                                            std::uint32_t count, SampleCount samples = SampleCount::One);

        static void TransformLayout(CommandManager manager,
                                    std::shared_ptr<_impl::Image_impl> data,
                                    VSL_NAMESPACE::ImageLayout oldLayout,
                                    VSL_NAMESPACE::ImageLayout newLayouts);
    };
} // namespace VSL_NAMESPACE
#endif
