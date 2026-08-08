//
// Created by morimoto_hibiki on 2025/10/07.
//

#ifndef AHO_ALL_BUFFER_HPP
#define AHO_ALL_BUFFER_HPP


#include "AHO/define.hpp"
#include "VSL/vulkan/buffer.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

namespace AHO_NAMESPACE::resource {
    enum class BufferType {
        General,
        GPUFriendly,
        OnGPU,
        Custom
    };

    template<BufferType Type>
    class Buffer {
    };

    // ===========================================

    template<>
    class Buffer<BufferType::General> {
        size_t m_size = 0;
        std::unique_ptr<char[]> m_buffer;
    public:
        Buffer(size_t size);
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) noexcept = default;
        Buffer& operator=(Buffer&&) noexcept = default;

        // ------

        // ------

        void* data();
        size_t size();
    };

    // ===========================================

    template<>
    class Buffer<BufferType::GPUFriendly> {
        // VSL_NAMESPACE::StagingBuffer<> buffer;
    public:
        Buffer(size_t size);

        // ------

        // ------

        void* data();
        size_t size();
    };

    // ===========================================

    template<>
    class Buffer<BufferType::OnGPU> {
        Buffer(size_t size);

    };

    // ===========================================

    inline Buffer<BufferType::General>::Buffer(size_t size) {
        m_size = size;
        m_buffer = std::make_unique<char[]>(size);
    }

    inline void *Buffer<BufferType::General>::data() {
        return m_buffer.get();
    }

    inline size_t Buffer<BufferType::General>::size() {
        return m_size;
    }

    // ===========================================

    inline Buffer<BufferType::GPUFriendly>::Buffer(size_t size) {
        //buffer = decltype(buffer)();
    }

    inline void *Buffer<BufferType::GPUFriendly>::data() {
    }

    inline size_t Buffer<BufferType::GPUFriendly>::size() {
    }
}


#endif //AHO_ALL_BUFFER_HPP
