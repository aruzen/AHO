//
// Created by morimoto_hibiki on 2025/10/07.
//

#ifndef AHO_ALL_BUFFER_HPP
#define AHO_ALL_BUFFER_HPP

#include "AHO/define.hpp"
#include "VSL/vulkan/buffer.hpp"

#include <algorithm>
#include <memory>

namespace AHO_NAMESPACE::resource {
    enum class BufferType {
        GENERAL,
        GPU_FRIENDLY,
        ON_GPU,
        CUSTOM
    };

    template<BufferType type>
    class Buffer {
    };

    // ===========================================

    template<>
    class Buffer<BufferType::GENERAL> {
        size_t _size;
        std::shared_ptr<char[]> _buffer;

    public:
        Buffer(size_t size);

        // ------

        // ------

        void* data();
        size_t size();
    };

    // ===========================================

    template<>
    class Buffer<BufferType::GPU_FRIENDLY> {
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
    class Buffer<BufferType::ON_GPU> {
        Buffer(size_t size);
    };

    // ===========================================

    inline Buffer<BufferType::GENERAL>::Buffer(size_t size) : _size(size), _buffer(new char[size]) {}

    inline void* Buffer<BufferType::GENERAL>::data() {
        return _buffer.get();
    }

    inline size_t Buffer<BufferType::GENERAL>::size() {
        return _buffer ? _size : 0;
    }

    // ===========================================

    inline Buffer<BufferType::GPU_FRIENDLY>::Buffer(size_t size) {
        //buffer = decltype(buffer)();
    }

    inline void* Buffer<BufferType::GPU_FRIENDLY>::data() {
    }

    inline size_t Buffer<BufferType::GPU_FRIENDLY>::size() {
    }
} // namespace AHO_NAMESPACE::resource

#endif //AHO_ALL_BUFFER_HPP
