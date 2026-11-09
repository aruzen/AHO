#include <VSL/utils/typed_block_validation.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/utils/spir_reflector.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <algorithm>

namespace VSL_NAMESPACE::typed::detail {
    void freeze_descriptor_dependencies(const GraphicResource& resource) {
        const auto data = resource.data_;
        if (!data || !data->owner_context)
            throw std::invalid_argument("Descriptor freeze requires a managed resource");
        if (!data->owner_node || data->owner_node->retiring() || !data->owner_node->matches(data) ||
            data->owner_node != &data->owner_context->node)
            throw std::invalid_argument("Descriptor freeze requires its canonical owner");
        if (!data->owner_context->single_resource)
            throw std::invalid_argument("Freeze all arena descriptors through ResourceArena");
        const std::array sets{data.get()};
        lifetime::freeze_descriptor_dependencies(*data->owner_context, sets);
    }
    lifetime::OwnedRef<impl::ImageImpl> descriptor_owner(const ImageAccessor& image) {
        return lifetime::detail::owned_ref(image.data_, image.data_ ? image.data_->owner_node : nullptr);
    }
    lifetime::OwnedRef<impl::SamplerImpl> descriptor_owner(const SamplerAccessor& sampler) {
        return lifetime::detail::owned_ref(sampler.data_, sampler.data_ ? sampler.data_->owner_node : nullptr);
    }
    ResourceUse descriptor_constraints(const DescriptorValue& value) {
        const auto matches = [&](const auto& owner) {
            if (!owner || !value.use.state.use_count() || value.use.state.get() != owner->use)
                return false;
            const auto same = [&](const auto& other) { return !owner.owner_before(other) && !other.owner_before(owner); };
            return same(value.use.state) && (!value.use.owner.use_count() || same(value.use.owner));
        };
        if (matches(value.buffer) || matches(value.image) || matches(value.sampler))
            return {value.use.constraint_state, value.use.constraint_owner};
        return value.use;
    }
    void require_descriptor_complete(const GraphicResource& resource) {
        if (!resource.data_)
            throw std::logic_error("Typed descriptor resource has been destroyed");
        lifetime::require_descriptor_complete(*resource.data_);
    }
    void validate_descriptor_resource(const GraphicResource& resource, LogicalDeviceAccessor device, std::span<const graphic_resource::BindingPoint> points) {
        lifetime::require_device_owner(device);
        if (!resource.data_ || !resource.data_.use_count())
            throw std::invalid_argument("Typed descriptor import requires a resource");
        lifetime::require_descriptor_valid(*resource.data_);
        const auto& target = *resource.data_;
        if (!device.data_ || !device.data_.use_count() || target.pool->device != device.data_.get() || !target.layout)
            throw std::invalid_argument("Typed descriptor import device/layout mismatch");
        operation::validate_descriptor_layout(*target.layout, *device.data_, points);
    }
    GraphicResource allocate_descriptor_set(LogicalDeviceAccessor device, graphic_resource::ManagerInterface& manager,
                                            std::span<const graphic_resource::BindingPoint> points) {
        graphic_resource::BindingLayout layout(device, std::vector<graphic_resource::BindingPoint>(points.begin(), points.end()));
        auto [pools, resources] = manager.allocate({layout});
        if (resources.size() != 1)
            throw std::runtime_error("Typed descriptor allocation failed");
        return std::move(resources.front());
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
            element.constraint = lifetime::descriptor_constraint(descriptor_constraints(value));
            if (point.binding_type == UNIFORM_BUFFER || point.binding_type == STORAGE_BUFFER) {
                if (!value.buffer || value.buffer->device != device)
                    throw std::invalid_argument("Typed descriptor buffer device/usage mismatch");
                operation::validate_descriptor_buffer(*value.buffer, vk::DescriptorType(point.binding_type), value.offset, value.bytes);
                buffers.push_back({value.buffer->buffer, value.offset, value.bytes});
                element.states.push_back(value.buffer->use);
                element.owners.push_back(value.buffer);
            } else {
                vk::DescriptorImageInfo info{};
                if (point.binding_type != SAMPLER) {
                    if (!value.image || value.image->device != device)
                        throw std::invalid_argument("Typed descriptor image device/usage mismatch");
                    operation::validate_descriptor_image(*value.image, vk::DescriptorType(point.binding_type));
                    info.imageView = value.image->view;
                    info.imageLayout = point.binding_type == STORAGE_IMAGE ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
                    element.states.push_back(value.image->use);
                    element.owners.push_back(value.image);
                }
                if (point.binding_type == SAMPLER || point.binding_type == COMBINED_IMAGE_SAMPLER) {
                    if (!value.sampler || value.sampler->device != device)
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
        const auto device = layout.get_device().data_;
        if (set_count > device->parent_device->device.getProperties().limits.maxBoundDescriptorSets)
            throw std::out_of_range("Typed descriptor set exceeds the device limit");
        std::vector<DescriptorLayoutCreateInfo> sets;
        sets.reserve(set_count);
        for (std::uint32_t set = 0; set < set_count; ++set) {
            std::vector<graphic_resource::BindingPoint> points;
            for (const auto& requirement : requirements)
                if (requirement.set == set)
                    points.push_back(requirement.point);
            sets.push_back({std::move(points)});
        }
        lifetime::replace_recipe_layouts(layout.edit_recipe(), layout.get_device(), {}, std::move(sets));
    }

    void validate_resource_schema(const lifetime::PipelineRecipeSnapshot& recipe, std::span<const DescriptorRequirement> requirements) {
        for (const auto& shader : recipe.definition().shaders) {
            if (shader->device != recipe.device().data_.get())
                throw std::invalid_argument("Typed shader/pipeline device mismatch");
            const auto stage = ShaderType(shader->stage);
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
