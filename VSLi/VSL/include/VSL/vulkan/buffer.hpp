#pragma once
#include "../define.hpp"

#include "buffer_and_image_accessor.hpp"

#include "../utils/flags.hpp"
#include <optional>
#include <cstring>

#include "command.hpp"
#include "../exceptions.hpp"

namespace VSL_NAMESPACE {
    namespace exceptions {
        struct MemoryNotHostVisibleException : VSL_NAMESPACE::exceptions::RuntimeException {
            MemoryNotHostVisibleException(std::string traceinfo = "");
            MemoryNotHostVisibleException(std::source_location sourece);
        };
    } // namespace exceptions

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode = SharingMode::Exclusive>
    struct Buffer : public BufferAccessor {
        constexpr static MemoryType Type = MemType;
        constexpr static MemoryProperty Property = MemProperty;
        constexpr static SharingMode Mode = ShareMode;

        Buffer(LogicalDeviceAccessor device, size_t size, std::optional<CommandManager> manager = std::nullopt);

        template<typename T>
        requires(!std::is_integral_v<T>) && (VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
        Buffer(LogicalDeviceAccessor device, T t);

        template<typename T>
        requires(!std::is_integral_v<T>) && (!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
        Buffer(LogicalDeviceAccessor device, CommandManager manager, T t);

        template<typename T>
        requires(VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
        Buffer(LogicalDeviceAccessor device, T* t, size_t size = 1);

        template<typename T>
        requires(!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
        Buffer(LogicalDeviceAccessor device, CommandManager manager, T* t, size_t size = 1);

        virtual LocalBufferHolder data(std::optional<size_t> size = std::nullopt, size_t offset = 0);
        virtual void flush(LocalBufferHolder& holder);
    };

    template<MemoryType MemType = MemoryType::None,
             MemoryProperty MemProperty = MemoryProperty::None,
             SharingMode ShareMode = SharingMode::Exclusive>
    struct StagingBuffer : public Buffer<MemoryType::TransferSource | MemType,
                                         MemoryProperty::HostCoherent | MemoryProperty::HostVisible | MemProperty,
                                         ShareMode> {
        constexpr static MemoryType Type = MemoryType::TransferSource | MemType;
        constexpr static MemoryProperty Property = MemoryProperty::HostCoherent | MemoryProperty::HostVisible | MemProperty;
        constexpr static SharingMode Mode = ShareMode;

        using Buffer<Type, Property, Mode>::Buffer;
    };

    template<MemoryType MemType = MemoryType::None,
             MemoryProperty MemProperty = MemoryProperty::None,
             SharingMode ShareMode = SharingMode::Exclusive>
    struct DeviceLocalBuffer : public Buffer<MemoryType::TransferDestination | MemType,
                                             MemoryProperty::DeviceLocal | MemProperty,
                                             ShareMode> {
        constexpr static MemoryType Type = MemoryType::TransferDestination | MemType;
        constexpr static MemoryProperty Property = MemoryProperty::DeviceLocal | MemProperty;
        constexpr static SharingMode Mode = ShareMode;

        using Buffer<Type, Property, Mode>::Buffer;
    };

    // ==========================================================================================================

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, CommandManager manager, T& t);

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, T& t, std::optional<CommandManager> manager = std::nullopt) {
        if constexpr (not contain<Buffer::Property, MemoryProperty::HostVisible>()) {
            if (not manager)
                throw exceptions::RuntimeException("buffer", "Device-local initialization requires a command manager");
            return make_buffer<Buffer>(device, *manager, t);
        } else {
            Buffer buffer(device, sizeof(T));
            {
                auto mapped = buffer.data();
                mapped.copy(t);
                if constexpr (not contain<Buffer::Property, MemoryProperty::HostCoherent>())
                    mapped.flush();
            }
            return buffer;
        }
    }

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, CommandManager manager, T& t) {
        Buffer buffer(device, sizeof(T), manager);
        StagingBuffer<> stagingBuffer(device, sizeof(T));
        stagingBuffer.copy(t);
        buffer.copyByBuffer(manager, &stagingBuffer);
        return buffer;
    }

    namespace helper {
        bool copy_with_shift_offset(void* data, size_t maxSize, size_t& offset);

        template<typename T, typename... Args>
        bool copy_with_shift_offset(void* data, size_t maxSize, size_t& offset, const T& t, const Args&... args) {
            if (offset > maxSize || sizeof(T) > maxSize - offset)
                return false;
            memcpy(static_cast<char*>(data) + offset, &t, sizeof(T));
            offset += sizeof(T);
            return helper::copy_with_shift_offset(data, maxSize, offset, args...);
        }

        template<typename T>
        void uncheck_copy_with_shift_offset(void* data, size_t& offset, const T& t) {
            memcpy(static_cast<char*>(data) + offset, &t, sizeof(T));
            offset += sizeof(T);
        }
    } // namespace helper

    template<typename... Args>
    bool VSL_NAMESPACE::BufferAccessor::copy(const Args&... args) {
        auto buff = this->data();
        size_t offset = 0;
        return helper::copy_with_shift_offset(buff.data, buff.size, offset, args...);
    }

    template<typename... Args>
    bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copy(const Args&... args) {
        size_t offset = 0;
        return helper::copy_with_shift_offset(this->data, this->size, offset, args...);
    }

    template<typename T>
    bool BufferAccessor::copy(T* t, size_t size) {
        auto buff = this->data();
        if (size > buff.size / sizeof(T))
            return false;
        if (size == 0)
            return true;
        memcpy(buff.data, t, sizeof(T) * size);
        return true;
    }

    template<typename T>
    bool BufferAccessor::LocalBufferHolder::copy(T* t, size_t size) {
        if (size > this->size / sizeof(T))
            return false;
        if (size == 0)
            return true;
        memcpy(data, t, sizeof(T) * size);
        return true;
    }

    template<typename... Args>
    void VSL_NAMESPACE::BufferAccessor::uncheck_copy(const Args&... args) {
        auto buff = data();
        size_t offset = 0;
        (helper::uncheck_copy_with_shift_offset(buff.data, offset, args), ...);
    }

    template<typename... Args>
    void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::uncheck_copy(const Args&... args) {
        size_t offset = 0;
        (helper::uncheck_copy_with_shift_offset(this->data, offset, args), ...);
    }

    template<VSL_NAMESPACE::MemoryType MemType, VSL_NAMESPACE::MemoryProperty MemProperty, VSL_NAMESPACE::SharingMode SharingMode>
    VSL_NAMESPACE::Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, size_t size, std::optional<CommandManager> manager) {
        _data = this->MakeBuffer(device, size, MemType, MemProperty, SharingMode);
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires(!std::is_integral_v<T>) && (VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, T t) {
        _data = this->MakeBuffer(device, sizeof(T), MemType, MemProperty, SharingMode);
        auto mapped = this->data();
        mapped.copy(t);
        if constexpr (not contain<Buffer::Property, MemoryProperty::HostCoherent>())
            mapped.flush();
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires(!std::is_integral_v<T>) && (!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, CommandManager manager, T t) {
        _data = this->MakeBuffer(device, sizeof(T), MemType, MemProperty, SharingMode);
        StagingBuffer<> stagingBuffer(device, sizeof(T));
        stagingBuffer.copy(t);
        this->copyByBuffer(manager, &stagingBuffer);
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires(VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, T* t, size_t size) {
        _data = this->MakeBuffer(device, sizeof(T) * size, MemType, MemProperty, SharingMode);
        {
            auto data = this->data();
            data.copy(t, size);
            if constexpr (not contain<Buffer::Property, MemoryProperty::HostCoherent>())
                data.flush();
        }
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires(!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, CommandManager manager, T* t, size_t size) {
        _data = this->MakeBuffer(device, sizeof(T) * size, MemType, MemProperty, SharingMode);
        StagingBuffer<> stagingBuffer(device, sizeof(T) * size);
        {
            auto data = stagingBuffer.data();
            bcopy(t, data.data, sizeof(T) * size);
        }
        this->copyByBuffer(manager, &stagingBuffer);
    }

    template<VSL_NAMESPACE::MemoryType MemType, VSL_NAMESPACE::MemoryProperty MemProperty, VSL_NAMESPACE::SharingMode SharingMode>
    VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::Buffer<MemType, MemProperty, SharingMode>::data(std::optional<size_t> size, size_t offset) {
        if constexpr (not contain<MemProperty, MemoryProperty::HostVisible>()) {
            static_assert("Not host-visible memory!");
        }

        return this->GetData(this, size, offset, MemType, MemProperty, SharingMode);
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    inline void Buffer<MemType, MemProperty, SharingMode>::flush(LocalBufferHolder& holder) {
        if constexpr (not contain<MemProperty, MemoryProperty::HostVisible>()) {
            static_assert("Not host-visible memory!");
        }

        this->FlushBuffer(holder, MemType, MemProperty, SharingMode);
    }
} // namespace VSL_NAMESPACE
