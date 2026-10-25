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

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode share_mode = SharingMode::EXCLUSIVE>
    struct Buffer : public BufferAccessor {
        constexpr static MemoryType TYPE = mem_type;
        constexpr static MemoryProperty PROPERTY = mem_property;
        constexpr static SharingMode MODE = share_mode;

        Buffer(LogicalDeviceAccessor device, size_t size, std::optional<CommandManager> manager = std::nullopt);

        template<typename T>
        requires(!std::is_integral_v<T>) && (VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
        Buffer(LogicalDeviceAccessor device, T t);

        template<typename T>
        requires(!std::is_integral_v<T>) && (!VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
        Buffer(LogicalDeviceAccessor device, CommandManager manager, T t);

        template<typename T>
        requires(VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
        Buffer(LogicalDeviceAccessor device, T* t, size_t size = 1);

        template<typename T>
        requires(!VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
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

    template<MemoryType mem_type = MemoryType::NONE,
             MemoryProperty mem_property = MemoryProperty::NONE,
             SharingMode share_mode = SharingMode::EXCLUSIVE>
    struct StagingBuffer : public Buffer<MemoryType::TRANSFER_SOURCE | mem_type,
                                         MemoryProperty::HOST_COHERENT | MemoryProperty::HOST_VISIBLE | mem_property,
                                         share_mode> {
        constexpr static MemoryType TYPE = MemoryType::TRANSFER_SOURCE | mem_type;
        constexpr static MemoryProperty PROPERTY = MemoryProperty::HOST_COHERENT | MemoryProperty::HOST_VISIBLE | mem_property;
        constexpr static SharingMode MODE = share_mode;

        using Buffer<TYPE, PROPERTY, MODE>::Buffer;
    };

    template<MemoryType mem_type = MemoryType::NONE,
             MemoryProperty mem_property = MemoryProperty::NONE,
             SharingMode share_mode = SharingMode::EXCLUSIVE>
    struct DeviceLocalBuffer : public Buffer<MemoryType::TRANSFER_DESTINATION | mem_type,
                                             MemoryProperty::DEVICE_LOCAL | mem_property,
                                             share_mode> {
        constexpr static MemoryType TYPE = MemoryType::TRANSFER_DESTINATION | mem_type;
        constexpr static MemoryProperty PROPERTY = MemoryProperty::DEVICE_LOCAL | mem_property;
        constexpr static SharingMode MODE = share_mode;

        using Buffer<TYPE, PROPERTY, MODE>::Buffer;
    };

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, CommandManager manager, T& t);

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, T& t, std::optional<CommandManager> manager = std::nullopt);

    namespace helper {
        template<typename T>
        constexpr std::size_t checked_buffer_array_size(std::size_t count);

        bool copy_with_shift_offset(void* data, size_t max_size, size_t& offset);

        template<typename T, typename... Args>
        bool copy_with_shift_offset(void* data, size_t max_size, size_t& offset, const T& t, const Args&... args);

        template<typename T>
        void uncheck_copy_with_shift_offset(void* data, size_t& offset, const T& t);
    } // namespace helper

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, T& t, std::optional<CommandManager> manager) {
        if constexpr (not contain<Buffer::PROPERTY, MemoryProperty::HOST_VISIBLE>()) {
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
        StagingBuffer<> staging_buffer(device, sizeof(T));
        staging_buffer.copy(t);
        buffer.copy_by_buffer(manager, &staging_buffer);
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
        bool copy_with_shift_offset(void* data, size_t max_size, size_t& offset, const T& t, const Args&... args) {
            if (offset > max_size || sizeof(T) > max_size - offset)
                return false;
            memcpy(static_cast<char*>(data) + offset, &t, sizeof(T));
            offset += sizeof(T);
            return helper::copy_with_shift_offset(data, max_size, offset, args...);
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
        flush_copied(buff, offset);
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
        std::memmove(buff.data, t, sizeof(T) * size);
        flush_copied(buff, sizeof(T) * size);
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
        std::memmove(data, t, sizeof(T) * size);
        return true;
    }

    template<typename... Args>
    void VSL_NAMESPACE::BufferAccessor::uncheck_copy(const Args&... args) {
        auto buff = data();
        size_t offset = 0;
        (helper::uncheck_copy_with_shift_offset(buff.data, offset, args), ...);
        flush_copied(buff, offset);
    }

    template<typename... Args>
    void VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::uncheck_copy(const Args&... args) {
        size_t offset = 0;
        (helper::uncheck_copy_with_shift_offset(this->data, offset, args), ...);
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode share_mode>
    template<typename... Args>
    bool Buffer<mem_type, mem_property, share_mode>::copy(const Args&... args) {
        if constexpr (not contain<mem_property, MemoryProperty::HOST_COHERENT>()) {
            return BufferAccessor::copy(args...);
        } else {
            auto buff = this->data();
            return buff.copy(args...);
        }
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode share_mode>
    template<typename T>
    bool Buffer<mem_type, mem_property, share_mode>::copy(T* t, size_t size) {
        if constexpr (not contain<mem_property, MemoryProperty::HOST_COHERENT>()) {
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

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode share_mode>
    template<typename... Args>
    void Buffer<mem_type, mem_property, share_mode>::uncheck_copy(const Args&... args) {
        if constexpr (not contain<mem_property, MemoryProperty::HOST_COHERENT>()) {
            BufferAccessor::uncheck_copy(args...);
        } else {
            auto buff = this->data();
            buff.uncheck_copy(args...);
        }
    }

    template<VSL_NAMESPACE::MemoryType mem_type, VSL_NAMESPACE::MemoryProperty mem_property, VSL_NAMESPACE::SharingMode sharing_mode>
    VSL_NAMESPACE::Buffer<mem_type, mem_property, sharing_mode>::Buffer(LogicalDeviceAccessor device, size_t size, std::optional<CommandManager> manager) {
        data_ = this->MakeBuffer(device, size, mem_type, mem_property, sharing_mode);
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode>
    template<typename T>
    requires(!std::is_integral_v<T>) && (VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
    Buffer<mem_type, mem_property, sharing_mode>::Buffer(LogicalDeviceAccessor device, T t) {
        data_ = this->MakeBuffer(device, sizeof(T), mem_type, mem_property, sharing_mode);
        this->copy(t);
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode>
    template<typename T>
    requires(!std::is_integral_v<T>) && (!VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
    Buffer<mem_type, mem_property, sharing_mode>::Buffer(LogicalDeviceAccessor device, CommandManager manager, T t) {
        data_ = this->MakeBuffer(device, sizeof(T), mem_type, mem_property, sharing_mode);
        StagingBuffer<> staging_buffer(device, sizeof(T));
        staging_buffer.copy(t);
        this->copy_by_buffer(manager, &staging_buffer);
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode>
    template<typename T>
    requires(VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
    Buffer<mem_type, mem_property, sharing_mode>::Buffer(LogicalDeviceAccessor device, T* t, size_t size) {
        const auto bytes = helper::checked_buffer_array_size<T>(size);
        data_ = this->MakeBuffer(device, bytes, mem_type, mem_property, sharing_mode);
        this->copy(t, size);
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode>
    template<typename T>
    requires(!VSL_NAMESPACE::contain<mem_property, MemoryProperty::HOST_VISIBLE>())
    Buffer<mem_type, mem_property, sharing_mode>::Buffer(LogicalDeviceAccessor device, CommandManager manager, T* t, size_t size) {
        const auto bytes = helper::checked_buffer_array_size<T>(size);
        data_ = this->MakeBuffer(device, bytes, mem_type, mem_property, sharing_mode);
        StagingBuffer<> staging_buffer(device, bytes);
        {
            auto data = staging_buffer.data();
            std::memcpy(data.data, t, bytes);
        }
        this->copy_by_buffer(manager, &staging_buffer);
    }

    template<VSL_NAMESPACE::MemoryType mem_type, VSL_NAMESPACE::MemoryProperty mem_property, VSL_NAMESPACE::SharingMode sharing_mode>
    VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::Buffer<mem_type, mem_property, sharing_mode>::data(std::optional<size_t> size, size_t offset) {
        if constexpr (not contain<mem_property, MemoryProperty::HOST_VISIBLE>()) {
            static_assert("Not host-visible memory!");
        }

        return this->GetData(this, size, offset, mem_type, mem_property, sharing_mode);
    }

    template<MemoryType mem_type, MemoryProperty mem_property, SharingMode sharing_mode>
    inline void Buffer<mem_type, mem_property, sharing_mode>::flush(LocalBufferHolder& holder) {
        if constexpr (not contain<mem_property, MemoryProperty::HOST_VISIBLE>()) {
            static_assert("Not host-visible memory!");
        }

        this->FlushBuffer(holder, mem_type, mem_property, sharing_mode);
    }
} // namespace VSL_NAMESPACE
