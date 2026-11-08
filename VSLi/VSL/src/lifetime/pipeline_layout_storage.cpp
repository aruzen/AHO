#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::PipelineLayoutImpl>);
    namespace {
        struct HeapPipelineLayoutOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(PipelineLayoutSlotStorage) std::byte storage[sizeof(PipelineLayoutSlotStorage)];
            HeapPipelineLayoutOwner(LogicalDeviceAccessor device, std::span<impl::ResourceBindingLayoutImpl* const> layouts,
                                    std::span<const PushConstantRange> ranges);
            ~HeapPipelineLayoutOwner();
        };
        HeapPipelineLayoutOwner::HeapPipelineLayoutOwner(LogicalDeviceAccessor device, std::span<impl::ResourceBindingLayoutImpl* const> layouts,
                                                         std::span<const PushConstantRange> ranges) : device(device) {
            construct_pipeline_layout(storage, device, layouts, ranges);
        }
        HeapPipelineLayoutOwner::~HeapPipelineLayoutOwner() {
            node.retire();
            destroy_pipeline_layout(storage);
        }
    } // namespace
    size_t retain_recipe_owner(std::vector<OwnerRef>& owners, OwnerRef owner) {
        detail::retain_owner(owners, owner);
        for (size_t i = 0; i < owners.size(); ++i)
            if (!owners[i].owner_before(owner) && !owner.owner_before(owners[i]))
                return i;
        throw std::logic_error("Recipe source owner was not retained");
    }
    void construct_pipeline_layout(void* storage, LogicalDeviceAccessor device,
                                   std::span<impl::ResourceBindingLayoutImpl* const> layouts, std::span<const PushConstantRange> ranges) {
        if (!device.data_ || !device.data_.use_count())
            throw std::invalid_argument("Pipeline layout requires a logical device owner");
        auto* slot = std::construct_at(static_cast<PipelineLayoutSlotStorage*>(storage));
        auto* layout = std::construct_at(reinterpret_cast<impl::PipelineLayoutImpl*>(slot->value));
        try {
            operation::initialize_pipeline_layout(*layout, *device.data_, layouts, ranges);
        } catch (...) {
            std::destroy_at(layout);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::PipelineLayoutImpl& pipeline_layout_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::PipelineLayoutImpl*>(static_cast<PipelineLayoutSlotStorage*>(storage)->value));
    }
    void destroy_pipeline_layout(void* storage) noexcept {
        std::destroy_at(&pipeline_layout_at(storage));
        std::destroy_at(static_cast<PipelineLayoutSlotStorage*>(storage));
    }
    std::shared_ptr<impl::PipelineLayoutImpl> make_heap_pipeline_layout(LogicalDeviceAccessor device,
                                                                        std::span<impl::ResourceBindingLayoutImpl* const> layouts, std::span<const PushConstantRange> ranges) {
        auto owner = std::make_shared<HeapPipelineLayoutOwner>(device, layouts, ranges);
        auto* layout = &pipeline_layout_at(owner->storage);
        return detail::publish_leaf(std::move(owner), layout);
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::PipelineLayoutImpl::~PipelineLayoutImpl() {
    operation::destroy_pipeline_layout(*this);
}
