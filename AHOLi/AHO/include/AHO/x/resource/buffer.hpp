#pragma once
#include <AHO/define.hpp>
#include <VSL/vulkan/command.hpp>
#include <cstddef>
#include <memory>
#include <span>

namespace AHO_NAMESPACE::x::resource {
    // コピーしたResourceはデータと容量を共有する。spanはreserve/resize前まで有効。
    class Buffer {
        struct Data;
        std::shared_ptr<Data> _data;

    public:
        explicit Buffer(std::size_t size = 0);
        std::size_t size() const;
        std::size_t capacity() const;
        std::span<std::byte> data();
        std::span<const std::byte> data() const;
        void reserve(std::size_t capacity);
        void resize(std::size_t size);
        void write(std::span<const std::byte> bytes, std::size_t offset = 0);
    };

    class Texture;
    struct TextureUpdate;
    // 転送用のGPUバッファを常時マップして再利用する。コピーは使用状態も共有する。
    class UploadBuffer {
        struct Data;
        std::shared_ptr<Data> _data;
        friend class Texture;
        friend struct TextureUpdate;

    public:
        explicit UploadBuffer(VSL_NAMESPACE::LogicalDeviceAccessor device, std::size_t capacity = 0);
        std::size_t size() const;
        std::size_t capacity() const;
        void reserve(std::size_t capacity);
        void assign(std::span<const std::byte> bytes);
        // 有効データの範囲内を書き換える。使用中はreserve/assign/writeを拒否する。
        void write(std::span<const std::byte> bytes, std::size_t offset = 0);
        bool busy() const;
        bool wait(std::uint64_t timeout_ns = UINT64_MAX) const;
    };
} // namespace AHO_NAMESPACE::x::resource
