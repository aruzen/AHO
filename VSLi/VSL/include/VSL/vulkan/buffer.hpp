#pragma once
#include "../define.hpp"

#include "buffer_and_image_accessor.hpp"

#include "../utils/flags.hpp"
#include <optional>
#include <cstring>

#include "../exceptions.hpp"

namespace VSL_NAMESPACE {
	namespace exceptions {
		struct MemoryNotHostVisibleException : VSL_NAMESPACE::exceptions::RuntimeException {
			MemoryNotHostVisibleException(std::string traceinfo = "");
			MemoryNotHostVisibleException(std::source_location sourece);
		};
	}

	template <MemoryType MemType, MemoryProperty MemProperty, SharingMode ShareMode = SharingMode::Exclusive>
	struct Buffer : public BufferAccessor {
        constexpr static MemoryType Type = MemType;
        constexpr static MemoryProperty Property = MemProperty;
        constexpr static SharingMode Mode = ShareMode;

		Buffer(LogicalDeviceAccessor device, size_t size, std::optional<CommandManager> manager = std::nullopt);

        template<typename T>
        requires (!std::is_integral_v<T>) && (VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
        Buffer(LogicalDeviceAccessor device, T t);

        template<typename T>
        requires (!std::is_integral_v<T>) && (!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
        Buffer(LogicalDeviceAccessor device, CommandManager manager, T t);

        virtual LocalBufferHolder data(std::optional<size_t> size = std::nullopt, size_t offset = 0);
		virtual void flush(LocalBufferHolder holder);
	};

    template <MemoryType MemType = MemoryType::None,
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

	template <MemoryType MemType = MemoryType::None,
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
    Buffer make_buffer(LogicalDeviceAccessor device, T& t, std::optional<CommandManager> manager = std::nullopt) {
        Buffer buffer(device, sizeof (T));
        buffer.copy(t);
        if constexpr (not contain<Buffer::Property, MemoryProperty::HostCoherent>()) {
            buffer.flush();
        }
        return buffer;
    }

    template<typename Buffer, typename T>
    Buffer make_buffer(LogicalDeviceAccessor device, CommandManager manager, T& t) {
        Buffer buffer(device, manager, sizeof (T));
        StagingBuffer<> stagingBuffer(device, sizeof (T));
        stagingBuffer.copy(t);
        buffer.copyByBuffer(manager, &stagingBuffer);
        return buffer;
    }

	namespace helper {
		bool copy_with_shift_offset(void* data, size_t maxSize, size_t& offset);

		template<typename T, typename... Args>
		bool copy_with_shift_offset(void* data, size_t maxSize, size_t& offset, const T& t, const Args&... args) {
			if (maxSize < offset + sizeof(T))
				return false;
			memcpy(data, &t, sizeof(T));
			return helper::copy_with_shift_offset(data, maxSize, offset, args...);
		}

		template<typename T>
		void uncheck_copy_with_shift_offset(void* data, size_t& offset, const T& t) {
			memcpy(data, &t, sizeof(T));
		}
	}

	template<typename... Args>
	bool VSL_NAMESPACE::BufferAccessor::copy(const Args&... args) {
		auto buff = this->data();
		size_t offset = 0;
		return helper::copy_with_shift_offset(buff.data, size(), offset, args...);
	}

    template<typename... Args>
    bool VSL_NAMESPACE::BufferAccessor::LocalBufferHolder::copy(const Args&... args) {
        size_t offset = 0;
        return helper::copy_with_shift_offset(this->data, parent->size(), offset, args...);
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

	template <VSL_NAMESPACE::MemoryType MemType, VSL_NAMESPACE::MemoryProperty MemProperty, VSL_NAMESPACE::SharingMode SharingMode>
	VSL_NAMESPACE::Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, size_t size, std::optional<CommandManager> manager) {
		_data = this->MakeBuffer(device, size, MemType, MemProperty, SharingMode);
	}

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires (!std::is_integral_v<T>) && (VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, T t) {
        _data = this->MakeBuffer(device, sizeof(T), MemType, MemProperty, SharingMode);
        this->copy(t);
        if constexpr (not contain<Buffer::Property, MemoryProperty::HostCoherent>()) {
            this->flush();
        }
    }

    template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
    template<typename T>
    requires (!std::is_integral_v<T>) && (!VSL_NAMESPACE::contain<MemProperty, MemoryProperty::HostVisible>())
    Buffer<MemType, MemProperty, SharingMode>::Buffer(LogicalDeviceAccessor device, CommandManager manager, T t) {
        _data = this->MakeBuffer(device, sizeof(T), MemType, MemProperty, SharingMode);
        StagingBuffer<> stagingBuffer(device, sizeof (T));
        stagingBuffer.copy(t);
        this->copyByBuffer(manager, &stagingBuffer);
    }

    template <VSL_NAMESPACE::MemoryType MemType, VSL_NAMESPACE::MemoryProperty MemProperty, VSL_NAMESPACE::SharingMode SharingMode>
	VSL_NAMESPACE::BufferAccessor::LocalBufferHolder VSL_NAMESPACE::Buffer<MemType, MemProperty, SharingMode>::data(std::optional<size_t> size, size_t offset) {
		if constexpr (not contain<MemProperty, MemoryProperty::HostVisible>()) {
			static_assert("Not host-visible memory!");
		}

		return this->GetData(this, size, offset, MemType, MemProperty, SharingMode);
	}

	template<MemoryType MemType, MemoryProperty MemProperty, SharingMode SharingMode>
	inline void Buffer<MemType, MemProperty, SharingMode>::flush(LocalBufferHolder holder)
	{
		if constexpr (not contain<MemProperty, MemoryProperty::HostVisible>()) {
			static_assert("Not host-visible memory!");
		}

		this->FlushBuffer(holder, MemType, MemProperty, SharingMode);
	}
}