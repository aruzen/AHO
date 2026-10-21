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
        NONE = 0,
        // データ転送の「送信元」バッファ。他のバッファや画像へデータをコピーするために使用。
        TRANSFER_SOURCE = 0x00000001,

        // データ転送の「受信先」バッファ。他のバッファや画像からデータを受け取るために使用。
        TRANSFER_DESTINATION = 0x00000002,

        // シェーダーがテクスチャのように読み込めるが、変更されないバッファ。
        UNIFORM_TEXEL_BUFFER = 0x00000004,

        // シェーダーが自由に読み書きできるバッファ。
        STORAGE_TEXEL_BUFFER = 0x00000008,

        // シェーダーが参照する「読み取り専用」のデータバッファ。
        UNIFORM_BUFFER = 0x00000010,

        // シェーダーが自由に読み書きできるデータバッファ。
        STORAGE_BUFFER = 0x00000020,

        // 描画時に頂点の描画順序を指定するためのバッファ。
        INDEX_BUFFER = 0x00000040,

        // 頂点の座標や色、テクスチャ座標などを格納するバッファ。
        VERTEX_BUFFER = 0x00000080,

        // GPU が間接描画 (`vkCmdDrawIndirect`) を行うためのバッファ。
        INDIRECT_DRAW_BUFFER = 0x00000100,

        // シェーダーから GPU メモリのアドレスを直接取得できるバッファ。
        SHADER_DEVICE_ADDRESS = 0x00020000,

        // ハードウェアによる動画デコードの入力データを格納するバッファ。
        VIDEO_DECODE_SOURCE = 0x00002000,

        // ハードウェアによる動画デコードの出力データを格納するバッファ。
        VIDEO_DECODE_DESTINATION = 0x00004000,

        // 頂点シェーダーの出力を保存するためのバッファ。
        TRANSFORM_FEEDBACK_BUFFER = 0x00000800,

        // トランスフォームフィードバック時に記録されるカウンター情報を保存するバッファ。
        TRANSFORM_FEEDBACK_COUNTER = 0x00001000,

        // GPU が条件に応じて描画をスキップできるようにするバッファ。
        CONDITIONAL_RENDERING = 0x00000200,

        // レイトレーシングの衝突判定 (BVH) を構築するための入力データ。
        ACCELERATION_STRUCTURE_BUILD_INPUT = 0x00080000,

        // レイトレーシング用の空間構造 (BVH) を格納するバッファ。
        ACCELERATION_STRUCTURE_STORAGE = 0x00100000,

        // レイトレーシングシェーダーを管理するための特別なバッファ。
        SHADER_BINDING_TABLE = 0x00000400,

        // ハードウェアによる動画エンコードの出力データを格納するバッファ。
        VIDEO_ENCODE_DESTINATION = 0x00008000,

        // ハードウェアによる動画エンコードの入力データを格納するバッファ。
        VIDEO_ENCODE_SOURCE = 0x00010000,

        // サンプラーの情報 (テクスチャのフィルタリング設定など) を格納するバッファ。
        SAMPLER_DESCRIPTOR_BUFFER = 0x00200000,

        // リソース (テクスチャやバッファ) の情報を格納するデスクリプタバッファ。
        RESOURCE_DESCRIPTOR_BUFFER = 0x00400000,

        // プッシュデスクリプタの情報を格納するバッファ。
        PUSH_DESCRIPTOR_BUFFER = 0x04000000,

        // マイクロマップ (微細なジオメトリデータ) を構築するための入力データを格納するバッファ。
        MICROMAP_BUILD_INPUT = 0x00800000,

        // マイクロマップデータ (ジオメトリの詳細情報) を格納するバッファ。
        MICROMAP_STORAGE = 0x01000000,
    };

    enum class SampleCount : std::uint32_t {
        ONE = 1,
        TWO = 2,
        FOUR = 4,
        EIGHT = 8,
        SIXTEEN = 16,
        THIRTY_TWO = 32,
        SIXTY_FOUR = 64
    };

    enum class ImageType {
        NONE = 0,
        TRANSFER_SRC = 0x00000001,
        TRANSFER_DST = 0x00000002,
        SAMPLED = 0x00000004,
        STORAGE = 0x00000008,
        COLOR_ATTACHMENT = 0x00000010,
        DEPTH_STENCIL_ATTACHMENT = 0x00000020,
        TRANSIENT_ATTACHMENT = 0x00000040,
        INPUT_ATTACHMENT = 0x00000080,
        HOST_TRANSFER = 0x00400000,
        VIDEO_DECODE_DST = 0x00000400,
        VIDEO_DECODE_SRC = 0x00000800,
        VIDEO_DECODE_DPB = 0x00001000,
        FRAGMENT_DENSITY_MAP = 0x00000200,
        FRAGMENT_SHADING_RATE_ATTACHMENT = 0x00000100,
        VIDEO_ENCODE_DST = 0x00002000,
        VIDEO_ENCODE_SRC = 0x00004000,
        VIDEO_ENCODE_DPB = 0x00008000,
        ATTACHMENT_FEEDBACK_LOOP = 0x00080000,
        VIDEO_ENCODE_QUANTIZATION_DELTA_MAP = 0x02000000,
        VIDEO_ENCODE_EMPHASIS_MAP = 0x04000000,
    };

    enum class MemoryProperty {
        NONE = 0,
        // GPUからのアクセスに適したメモリ。CPUからアクセスできるかはHostVisibleによる。
        DEVICE_LOCAL = 0x00000001,

        // CPU からアクセス可能なメモリ: CPU から直接読み書きできるメモリ。
        HOST_VISIBLE = 0x00000002,

        // CPUキャッシュの明示的なフラッシュ・無効化が不要。GPUの実行完了待ちは別途必要。
        HOST_COHERENT = 0x00000004,

        // CPU 側でキャッシュされるメモリ: CPU のキャッシュが有効で、読み取り速度が速くなる。
        HOST_CACHED = 0x00000008,

        // 遅延割り当てメモリ: メモリが必要になるまで実際に割り当てられない。
        LAZILY_ALLOCATED = 0x00000010,

        // 保護されたGPU処理で使用するメモリ。CPUからはアクセスできない。
        PROTECTED = 0x00000020,

        // AMD拡張による、デバイス側キャッシュの明示的な可視性操作が不要なメモリ。
        DEVICE_COHERENT_AMD = 0x00000040,

        // AMD 用のデバイス非キャッシュメモリ: GPU がキャッシュしないメモリ。
        DEVICE_UNCACHED_AMD = 0x00000080,

        // NVIDIA RDMA 対応メモリ: ネットワーク経由で直接メモリにアクセス可能なメモリ。
        RDMA_CAPABLE_NV = 0x00000100,
    };

    enum class ImageLayout {
        UNDEFINED = 0,
        GENERAL = 1,
        COLOR_ATTACHMENT_OPTIMAL = 2,
        DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
        DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
        SHADER_READ_ONLY_OPTIMAL = 5,
        TRANSFER_SRC_OPTIMAL = 6,
        TRANSFER_DST_OPTIMAL = 7,
        PREINITIALIZED = 8,
        PRESENT = 1000001002,
    };

    enum class SharingMode {
        EXCLUSIVE = 0,
        CONCURRENT = 1,
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

            bool copy_by_buffer(CommandManager command_manager, BufferAccessor* buf);

            bool copy_by_image(CommandManager command_manager, const ImageAccessor& image);

        private:
            std::shared_ptr<impl::BufferImpl> _owner;
            void synchronize(std::size_t bytes, std::size_t offset, bool flush) const;
        };

        std::shared_ptr<impl::BufferImpl> data_;

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

        bool copy_by_buffer(CommandManager command_manager, BufferAccessor* buf);

        bool copy_by_image(CommandManager command_manager, const ImageAccessor& image);

        size_t size();
        // 要求したバイト数を返す。
        std::size_t requested_size() const;

        static std::shared_ptr<impl::BufferImpl>
        MakeBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, size_t size, MemoryType mem_type,
                   MemoryProperty mem_property, SharingMode sharing_mode);

        static void
        FlushBuffer(LocalBufferHolder& holder, MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode);

        static LocalBufferHolder
        GetData(VSL_NAMESPACE::BufferAccessor* data, std::optional<size_t> size, size_t offset, MemoryType mem_type,
                MemoryProperty mem_property, SharingMode sharing_mode);

    private:
        void flush_copied(LocalBufferHolder& holder, std::size_t bytes);
    };

    struct ImageAccessor {
        std::shared_ptr<impl::ImageImpl> data_;
        data_format::Format get_format() const;
        ImageType get_usage() const;
        LogicalDeviceAccessor get_device() const;

        bool copy_by_buffer(CommandManager command_manager, BufferAccessor* buf, VSL_NAMESPACE::ImageLayout layout, std::uint32_t target_idx);

        size_t size();

        static std::shared_ptr<impl::ImageImpl> MakeImage(VSL_NAMESPACE::ImageType type,
                                                            VSL_NAMESPACE::MemoryProperty prop,
                                                            VSL_NAMESPACE::ImageLayout layout,
                                                            VSL_NAMESPACE::LogicalDeviceAccessor device,
                                                            std::uint32_t width,
                                                            std::uint32_t height,
                                                            data_format::Format format,
                                                            std::uint32_t count, SampleCount samples = SampleCount::ONE);

        static void TransformLayout(CommandManager manager,
                                    std::shared_ptr<impl::ImageImpl> data,
                                    VSL_NAMESPACE::ImageLayout old_layout,
                                    VSL_NAMESPACE::ImageLayout new_layouts);
    };
} // namespace VSL_NAMESPACE
#endif
