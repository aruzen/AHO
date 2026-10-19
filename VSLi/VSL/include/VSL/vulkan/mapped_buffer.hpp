#pragma once
#include "buffer_and_image_accessor.hpp"
#include <cstddef>
#include <span>

namespace VSL_NAMESPACE {
    // CPUから読み取れるGPUバッファを常時マップする。コピーしたオブジェクトは所有権を共有する。
    struct MappedBuffer {
        std::shared_ptr<_impl::MappedBuffer_impl> _data;
        explicit MappedBuffer(LogicalDeviceAccessor device, std::size_t bytes,
                              MemoryType usage = MemoryType::TransferDestination);
        std::size_t size() const;
        // GPUの書き込み完了を待ってから呼ぶ。非コヒーレントメモリのキャッシュはここで無効化する。
        // 戻り値の使用中はバッファを保持する。
        std::span<const std::byte> read(std::size_t bytes, std::size_t offset = 0) const;
    };
}
