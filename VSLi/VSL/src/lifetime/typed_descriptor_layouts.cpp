#include <VSL/lifetime/descriptor_access.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/typed_descriptor_layouts.hpp>
#include <VSL/operation/descriptor_ops.hpp>
#include <VSL/vulkan/stages/resource_binding.hpp>
#include <VSL/vulkan/_pimpls.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
}

namespace VSL_NAMESPACE::typed::detail {
    LogicalDeviceAccessor descriptor_layout_device(std::span<const graphic_resource::BindingLayout> layouts) {
        if (layouts.empty())
            return {};
        const auto& first = layouts.front();
        if (!DescriptorAccess::Owned(first) || !DescriptorAccess::Owned(first).use_count() || !DescriptorAccess::Owned(first)->layout || !DescriptorAccess::Owned(first)->device)
            throw std::invalid_argument("Typed layout collection requires owning live sources");
        return lifetime::device_from_child(DescriptorAccess::Owned(first));
    }
    void validate_descriptor_layout_source(LogicalDeviceAccessor device, const graphic_resource::BindingLayout& layout,
                                           std::span<const graphic_resource::BindingPoint> points) {
        lifetime::require_device_owner(device);
        if (!device.data_ || !device.data_.use_count() || !device.data_->device || !DescriptorAccess::Owned(layout) || !DescriptorAccess::Owned(layout).use_count())
            throw std::invalid_argument("Typed layout collection requires device and source owners");
        operation::validate_descriptor_layout(*DescriptorAccess::Owned(layout), *device.data_, points);
    }
    void inject_descriptor_layouts(PipelineLayoutEditAccessor layout, LogicalDeviceAccessor device,
                                   std::span<const graphic_resource::BindingLayout> sources, std::span<const std::uint32_t> numbers,
                                   const std::optional<graphic_resource::BindingLayout>& empty, std::uint32_t set_count) {
        const auto target = layout.get_device();
        if (sources.size() != numbers.size() || (!sources.empty() && target.data_ != device.data_))
            throw std::invalid_argument("Typed pipeline and layout sources require the same device");
        if (set_count > target.data_->parent_device->device.getProperties().limits.maxBoundDescriptorSets)
            throw std::out_of_range("Typed descriptor set exceeds the device limit");
        std::vector<graphic_resource::BindingLayout> sets;
        sets.reserve(set_count);
        for (std::uint32_t set = 0; set < set_count; ++set) {
            const auto found = std::ranges::find(numbers, set);
            if (found != numbers.end())
                sets.push_back(sources[static_cast<size_t>(found - numbers.begin())]);
            else if (empty)
                sets.push_back(*empty);
            else
                throw std::invalid_argument("Typed layout sources do not cover the native set indices");
        }
        pipeline_layout::ResourceBinding(std::move(sets)).injection(std::move(layout));
    }
} // namespace VSL_NAMESPACE::typed::detail
