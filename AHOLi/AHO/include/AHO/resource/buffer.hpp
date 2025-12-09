//
// Created by morimoto_hibiki on 2025/10/07.
//

#ifndef AHO_ALL_BUFFER_HPP
#define AHO_ALL_BUFFER_HPP


#include "AHO/define.hpp"
#include "VSL/vulkan/buffer.hpp"

#include <algorithm>

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
        size_t m_size;
        char * m_buffer;
    public:
        Buffer(size_t size);

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

    Buffer<BufferType::General>::Buffer(size_t size) {
        m_buffer = new char[size];
    }

    void *Buffer<BufferType::General>::data() {
        return m_buffer;
    }

    size_t Buffer<BufferType::General>::size() {
        return m_size;
    }

    // ===========================================

    Buffer<BufferType::GPUFriendly>::Buffer(size_t size) {
        //buffer = decltype(buffer)();
    }

    void *Buffer<BufferType::GPUFriendly>::data() {
    }

    size_t Buffer<BufferType::GPUFriendly>::size() {
    }
}


#endif //AHO_ALL_BUFFER_HPP
