#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace VSL_NAMESPACE::typed::detail {
    ImageAccessor BundleNativeResources::image(ResourceId id) const {
        for (const auto& [resource, value] : images)
            if (resource == id)
                return value;
        throw std::out_of_range("Unknown heap bundle image");
    }
    SamplerAccessor BundleNativeResources::sampler(ResourceId id) const {
        for (const auto& [resource, value] : samplers)
            if (resource == id)
                return value;
        throw std::out_of_range("Unknown heap bundle sampler");
    }
    void BundleNativeResources::require_idle() const {
        for (const auto& [id, value] : images)
            value.use_state().require_idle();
        for (const auto& [id, value] : samplers)
            value.use_state().require_idle();
    }
    BundleNativeResources make_bundle_native(LogicalDeviceAccessor device, std::span<const ResourcePlacement> placements, std::span<const BundleResourceInit> infos) {
        BundleNativeResources result;
        for (size_t index = 0; index < placements.size(); ++index) {
            const auto id = placements[index].resource;
            if (const auto* info = std::get_if<ImageCreateInfo>(&infos[index]))
                result.images.emplace_back(id, ImageAccessor{ImageAccessor::MakeImage(info->usage, info->properties, info->initial_layout, device, info->width, info->height, info->format, info->layers, info->samples)});
            else if (const auto* info = std::get_if<SamplerCreateInfo>(&infos[index]))
                result.samplers.emplace_back(id, Sampler(device, info->sampling, info->edge));
        }
        return result;
    }
    size_t bundle_frame_count(CommandManager manager) {
        return FrameResourceDomain(std::move(manager)).count;
    }
    void validate_bundle_plan(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager, std::span<const ResourceAllocationRecord> entries) {
        if (!device.data_ || !manager.data_ || manager.data_->device != device.data_)
            throw std::invalid_argument("Typed bundle descriptor manager device mismatch");
        const auto limits = device.data_->parent_device->device.getProperties().limits;
        for (const auto& entry : entries) {
            if (entry.declaration.external || !buffer_descriptor(entry.declaration.kind))
                continue;
            const auto bytes = allocation_multiply(entry.elements, entry.declaration.payload_size);
            const auto maximum = entry.declaration.kind == graphic_resource::Type::UNIFORM_BUFFER ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
            if (bytes > maximum)
                throw std::out_of_range("Typed bundle buffer exceeds the device descriptor range limit");
        }
    }
} // namespace VSL_NAMESPACE::typed::detail
