#include <VSL/utils/typed_block_validation.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <VSL/utils/spir_reflector.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <algorithm>

namespace VSL_NAMESPACE::typed::detail {
    ResourceUse descriptor_constraints(const DescriptorValue& value) {
        const auto* physical = value.use.state.get();
        if ((value.buffer && physical == value.buffer->use) ||
            (value.image && physical == value.image->use) ||
            (value.sampler && physical == value.sampler->use))
            return {value.use.constraint_state, value.use.constraint_owner};
        return value.use;
    }
    void require_descriptor_complete(const GraphicResource& resource) {
        if (!resource.data_)
            throw std::logic_error("Typed descriptor resource has been destroyed");
        lifetime::require_descriptor_complete(*resource.data_);
    }
    void write_descriptor(GraphicResource resource, graphic_resource::BindingPoint point, std::span<const DescriptorValue> values) {
        const auto& target = resource.data_;
        if (!target || !target->pool || !target->pool->descriptor_pool || !target->descriptor_set)
            throw std::logic_error("Typed descriptor resource has been destroyed");
        if (values.size() != point.amount)
            throw std::invalid_argument("Typed descriptor array size mismatch");
        const auto device = target->pool->device;
        std::vector<vk::DescriptorBufferInfo> buffers;
        std::vector<vk::DescriptorImageInfo> images;
        std::vector<impl::GraphicResourceImpl::BindingElement> elements;
        elements.reserve(values.size());
        buffers.reserve(values.size());
        images.reserve(values.size());
        using enum graphic_resource::Type;
        for (const auto& value : values) {
            impl::GraphicResourceImpl::BindingElement element;
            element.constraint = descriptor_constraints(value);
            if (point.binding_type == UNIFORM_BUFFER || point.binding_type == STORAGE_BUFFER) {
                if (!value.buffer || value.buffer->device != device.get())
                    throw std::invalid_argument("Typed descriptor buffer device/usage mismatch");
                operation::validate_descriptor_buffer(*value.buffer, vk::DescriptorType(point.binding_type), value.offset, value.bytes);
                buffers.push_back({value.buffer->buffer, value.offset, value.bytes});
                element.states.push_back(value.buffer->use);
                element.owners.push_back(value.buffer);
            } else {
                vk::DescriptorImageInfo info{};
                if (point.binding_type != SAMPLER) {
                    if (!value.image || value.image->device != device.get())
                        throw std::invalid_argument("Typed descriptor image device/usage mismatch");
                    operation::validate_descriptor_image(*value.image, vk::DescriptorType(point.binding_type));
                    info.imageView = value.image->view;
                    info.imageLayout = point.binding_type == STORAGE_IMAGE ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
                    element.states.push_back(value.image->use);
                    element.owners.push_back(value.image);
                }
                if (point.binding_type == SAMPLER || point.binding_type == COMBINED_IMAGE_SAMPLER) {
                    if (!value.sampler || value.sampler->device != device.get())
                        throw std::invalid_argument("Typed descriptor sampler device mismatch");
                    info.sampler = value.sampler->sampler;
                    element.states.push_back(value.sampler->use);
                    element.owners.push_back(value.sampler);
                }
                images.push_back(info);
            }
            elements.push_back(std::move(element));
        }
        vk::WriteDescriptorSet write{};
        write.dstSet = target->descriptor_set;
        write.dstBinding = point.binding;
        write.descriptorType = vk::DescriptorType(point.binding_type);
        write.descriptorCount = point.amount;
        write.pBufferInfo = buffers.empty() ? nullptr : buffers.data();
        write.pImageInfo = images.empty() ? nullptr : images.data();
        lifetime::write_descriptor_binding(*target, write, std::move(elements));
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
            validate_descriptor_blocks(reflection.generated, requirements);
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
