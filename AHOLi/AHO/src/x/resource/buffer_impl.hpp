#pragma once
#include <AHO/x/resource/buffer.hpp>
#include <VSL/vulkan/buffer.hpp>
#include <optional>
#include <stdexcept>

namespace AHO_NAMESPACE::x::resource {
    struct UploadBuffer::Data {
        VSL_NAMESPACE::LogicalDeviceAccessor device;
        using Buffer = VSL_NAMESPACE::Buffer<VSL_NAMESPACE::MemoryType::TransferSource, VSL_NAMESPACE::MemoryProperty::HostVisible>;
        std::optional<Buffer> buffer;
        std::optional<VSL_NAMESPACE::BufferAccessor::LocalBufferHolder> mapping;
        std::size_t size = 0;
        VSL_NAMESPACE::Completion use;
        explicit Data(VSL_NAMESPACE::LogicalDeviceAccessor device) : device(device) {}
        bool busy() const {
            const auto status = use.status();
            return status == VSL_NAMESPACE::Completion::Status::Pending || status == VSL_NAMESPACE::Completion::Status::Submitted;
        }
        void requireIdle() const {
            if (busy())
                throw std::logic_error("UploadBuffer is still in use");
        }
    };
} // namespace AHO_NAMESPACE::x::resource
