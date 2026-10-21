#pragma once
#include "../define.hpp"

#include "buffer_and_image_accessor.hpp"

#include "../utils/flags.hpp"
#include <optional>
#include <cstring>
#include <limits>

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

        template<typename... Args>
        bool copy(const Args&... args);

        template<typename T>
        bool copy(T* t, size_t size);

        template<typename... Args>
        void uncheck_copy(const Args&... args);

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

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, CommandManager manager, T& t);

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, T& t, std::optional<CommandManager> manager = std::nullopt);

    namespace helper {
        template<typename T>
        constexpr std::size_t checked_buffer_array_size(std::size_t count);

        bool copy_with_shift_offset(void* data, size_t maxSize, size_t& offset);

        template<typename T, typename... Args>
        bool copy_with_shift_offset(void* data, size_t maxSize, size_t& offset, const T& t, const Args&... args);

        template<typename T>
        void uncheck_copy_with_shift_offset(void* data, size_t& offset, const T& t);
    } // namespace helper

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, T& t, std::optional<CommandManager> manager) {
        if constexpr (not contain<Buffer::Property, MemoryProperty::HostVisible>()) {
            if (not manager)
                throw exceptions::RuntimeException("buffer", "Device-local initialization requires a command manager");
            return make_buffer<Buffer>(device, *manager, t);
        } else {
            Buffer buffer(device, sizeof(T));
            buffer.copy(t);
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
        template<typename T>
        constexpr std::size_t checked_buffer_array_size(std::size_t count) {
            if (count == 0)
                throw exceptions::RuntimeException("BufferSize", "Buffer arrays must contain at least one element");
            if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw exceptions::RuntimeException("BufferSize", "Buffer array byte size overflow");
            return sizeof(T) * count;
        }

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
        const bool copied = helper::copy_with_shift_offset(buff.data, buff.size, offset, args...);
        buff.flush(offset);
        return copied;
    }

    template<typename... Args>
    bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copy(const Args&... args) {
        size_t offset = 0;
        return helper::copy_with_shift_offset(this->data, this->size, offset, args...);
    }

    template<typename T, typename Count>
    requires(!std::same_as<Count, std::size_t> &&
             (std::integral<Count> || (std::is_enum_v<Count> && std::is_convertible_v<Count, std::size_t>)))
    bool BufferAccessor::copy(T* t, Count size) {
        if constexpr (std::is_enum_v<Count>) {
            return copy(t, static_cast<std::underlying_type_t<Count>>(size));
        } else {
            if constexpr (std::is_signed_v<Count>) {
                if (size < 0)
                    return false;
            }
            if constexpr (sizeof(Count) > sizeof(std::size_t)) {
                if (size > std::numeric_limits<std::size_t>::max())
                    return false;
            }
            return copy(t, static_cast<std::size_t>(size));
        }
    }

    template<typename T>
    bool BufferAccessor::copy(T* t, size_t size) {
        auto buff = this->data();
        if (size > buff.size / sizeof(T))
            return false;
        if (size == 0)
            return true;
        buff.write(std::as_bytes(std::span(t, size)));
        return true;
    }

    template<typename T, typename Count>
    requires(!std::same_as<Count, std::size_t> &&
             (std::integral<Count> || (std::is_enum_v<Count> && std::is_convertible_v<Count, std::size_t>)))
    bool BufferAccessor::LocalBufferHolder::copy(T* t, Count size) {
        if constexpr (std::is_enum_v<Count>) {
            return copy(t, static_cast<std::underlying_type_t<Count>>(size));
        } else {
            if constexpr (std::is_signed_v<Count>) {
                if (size < 0)
                    return false;
            }
            if constexpr (sizeof(Count) > sizeof(std::size_t)) {
                if (size > std::numeric_limits<std::size_t>::max())
                    return false;
            }
            return copy(t, static_cast<std::size_t>(size));
        }
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
        buff.flush(offset);
    }

    template<typename... Args>
    void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::uncheck_copy(const Args&... args) {
        size_t offset = 0;
        (helper::uncheck_copy_with_shift_offset(this->data, offset, args), ...);
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode>
    template<typename... Args>
    bool Buffer<MemType, MemProperty, ShareMode>::copy(const Args&... args) {
        if constexpr (not contain<MemProperty, MemoryProperty::HostCoherent>()) {
            return BufferAccessor::copy(args...);
        } else {
            auto buff = this->data();
            return buff.copy(args...);
        }
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode>
    template<typename T>
    bool Buffer<MemType, MemProperty, ShareMode>::copy(T* t, size_t size) {
        if constexpr (not contain<MemProperty, MemoryProperty::HostCoherent>()) {
            return BufferAccessor::copy(t, size);
        } else {
            auto buff = this->data();
            if (size > buff.size / sizeof(T))
                return false;
            if (size)
                std::memmove(buff.data, t, sizeof(T) * size);
            return true;
        }
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode>
    template<typename... Args>
    void Buffer<MemType, MemProperty, ShareMode>::uncheck_copy(const Args&... args) {
        if constexpr (not contain<MemProperty, MemoryProperty::HostCoherent>()) {
            BufferAccessor::uncheck_copy(args...);
        } else {
            auto buff = this->data();
            buff.uncheck_copy(args...);
        }
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
        this->copy(t);
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
        const auto bytes = helper::checked_buffer_array_size<T>(size);
        _data = this->MakeBuffer(device, bytes, MemType, MemProperty, SharingMode);
        this->copy(t, size);
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires(!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, CommandManager manager, T* t, size_t size) {
        const auto bytes = helper::checked_buffer_array_size<T>(size);
        _data = this->MakeBuffer(device, bytes, MemType, MemProperty, SharingMode);
        StagingBuffer<> stagingBuffer(device, bytes);
        {
            auto data = stagingBuffer.data();
            std::memcpy(data.data, t, bytes);
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
