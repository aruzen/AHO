#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::typed::detail {
    size_t bundle_frame_count(CommandManager manager) {
        return FrameResourceDomain(std::move(manager)).count;
    }
    void validate_bundle_plan(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, std::span<const ResourceAllocationRecord> entries) {
        if (!device.data_ || !manager.data_ || manager.data_->device != device.data_)
            throw std::invalid_argument("Typed bundle descriptor manager device mismatch");
        const auto limits = device.data_->parent_device->device.getProperties().limits;
        for (const auto& entry : entries) {
            if (entry.declaration.external)
                continue;
            const auto bytes = allocation_multiply(entry.elements, entry.declaration.payload_size);
            const auto maximum = entry.declaration.kind == graphic_resource::Type::UNIFORM_BUFFER ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
            if (bytes > maximum)
                throw std::out_of_range("Typed bundle buffer exceeds the device descriptor range limit");
        }
    }
} // namespace VSL_NAMESPACE::typed::detail
