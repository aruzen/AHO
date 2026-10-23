#include <VSL/utils/typed_resources.hpp>
#include <VSL/utils/spir_reflector.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <algorithm>

namespace VSL_NAMESPACE::typed::detail {
    void write_descriptor(GraphicResource resource, graphic_resource::BindingPoint point, std::span<const DescriptorValue> values) {
        const auto& target = resource.data_;
        if (!target || !target->pool || !target->pool->descriptor_pool || !target->descriptor_set)
            throw std::logic_error("Typed descriptor resource has been destroyed");
        if (values.size() != point.amount)
            throw std::invalid_argument("Typed descriptor array size mismatch");
        const auto device = target->pool->device;
        std::vector<vk::DescriptorBufferInfo> buffers;
        std::vector<vk::DescriptorImageInfo> images;
        std::vector<std::shared_ptr<void>> owners;
        buffers.reserve(values.size());
        images.reserve(values.size());
        using enum graphic_resource::Type;
        for (const auto& value : values) {
            if (point.binding_type == UNIFORM_BUFFER || point.binding_type == STORAGE_BUFFER) {
                const auto usage = point.binding_type == UNIFORM_BUFFER ? MemoryType::UNIFORM_BUFFER : MemoryType::STORAGE_BUFFER;
                if (!value.buffer || value.buffer->device != device || !contain(value.buffer->usage, usage))
                    throw std::invalid_argument("Typed descriptor buffer device/usage mismatch");
                if (!value.bytes || value.bytes > value.buffer->requirement_size)
                    throw std::out_of_range("Typed descriptor buffer range mismatch");
                const auto limits = device->parent_device->device.getProperties().limits;
                const auto limit = point.binding_type == UNIFORM_BUFFER ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
                if (value.bytes > limit)
                    throw std::out_of_range("Typed descriptor buffer exceeds the device range limit");
                buffers.push_back({value.buffer->buffer, 0, value.bytes});
                owners.push_back(value.buffer);
            } else {
                vk::DescriptorImageInfo info{};
                if (point.binding_type != SAMPLER) {
                    const auto usage = point.binding_type == STORAGE_IMAGE ? ImageType::STORAGE : ImageType::SAMPLED;
                    if (!value.image || value.image->device != device || !contain(value.image->usage, usage))
                        throw std::invalid_argument("Typed descriptor image device/usage mismatch");
                    info.imageView = value.image->view;
                    info.imageLayout = point.binding_type == STORAGE_IMAGE ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
                    owners.push_back(value.image);
                }
                if (point.binding_type == SAMPLER || point.binding_type == COMBINED_IMAGE_SAMPLER) {
                    if (!value.sampler || value.sampler->device != device)
                        throw std::invalid_argument("Typed descriptor sampler device mismatch");
                    info.sampler = value.sampler->sampler;
                    owners.push_back(value.sampler);
                }
                images.push_back(info);
            }
        }
        vk::WriteDescriptorSet write{};
        write.dstSet = target->descriptor_set;
        write.dstBinding = point.binding;
        write.descriptorType = vk::DescriptorType(point.binding_type);
        write.descriptorCount = point.amount;
        write.pBufferInfo = buffers.empty() ? nullptr : buffers.data();
        write.pImageInfo = images.empty() ? nullptr : images.data();
        target->bindings[point.binding] = std::move(owners);
        device->device.updateDescriptorSets(write, {});
    }

    void inject_resource_schema(PipelineLayoutEditAccessor layout, std::span<const DescriptorRequirement> requirements, std::uint32_t set_count) {
        const auto device = layout.data_->device;
        if (set_count > device->parent_device->device.getProperties().limits.maxBoundDescriptorSets)
            throw std::out_of_range("Typed descriptor set exceeds the device limit");
        std::vector<graphic_resource::BindingLayout> sets;
        sets.reserve(set_count);
        for (std::uint32_t set = 0; set < set_count; ++set) {
            std::vector<graphic_resource::BindingPoint> points;
            for (const auto& requirement : requirements)
                if (requirement.set == set)
                    points.push_back(requirement.point);
            sets.emplace_back(LogicalDeviceAccessor{device}, points);
        }
        pipeline_layout::ResourceBinding(std::move(sets)).injection(layout);
    }

    void validate_resource_schema(PipelineLayoutAccessor layout, std::span<const DescriptorRequirement> requirements) {
        for (const auto& shader : layout.data_->info->shaders) {
            if (shader->device != layout.data_->device)
                throw std::invalid_argument("Typed shader/pipeline device mismatch");
            const auto stage = ShaderType(shader->shader_stage_create_info.stage);
            utils::SPIRVReflector reflection({}, std::span<const std::uint32_t>(shader->code), shader->name, stage);
            for (const auto& [set, points] : reflection.generated.binding_points) {
                for (const auto& point : points) {
                    const auto found = std::ranges::find_if(requirements, [&](const auto& value) {
                        return value.set == set && value.point.binding == point.binding;
                    });
                    if (found == requirements.end() || found->point.binding_type != point.binding_type ||
                        found->point.amount != point.amount || !contain(found->point.shader_type, point.shader_type))
                        throw std::invalid_argument("Typed descriptor shader mismatch: entry '" + shader->name + "', set " +
                                                    std::to_string(set) + ", binding " + std::to_string(point.binding));
                }
            }
        }
    }
    void validate_dispatch(LogicalDeviceAccessor device, size_t x, size_t y, size_t z) {
        const auto limits = device.data_->parent_device->device.getProperties().limits;
        const std::array<size_t, 3> counts{x, y, z};
        for (size_t i = 0; i < counts.size(); ++i)
            if (counts[i] > limits.maxComputeWorkGroupCount[i])
                throw std::out_of_range("Typed dispatch exceeds the device workgroup count limit");
    }
} // namespace VSL_NAMESPACE::typed::detail
