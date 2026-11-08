#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/pipeline_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::PipelineImpl>);
    namespace {
        struct HeapPipelineOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            OwnerRef layout;
            alignas(PipelineSlotStorage) std::byte storage[sizeof(PipelineSlotStorage)];
            template<typename Input>
            HeapPipelineOwner(LogicalDeviceAccessor device, OwnerRef layout, const Input& input);
            ~HeapPipelineOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        template<typename Input>
        void construct(void* storage, impl::LogicalDeviceImpl& device, const Input& input);
        template<typename Input>
        std::shared_ptr<impl::PipelineImpl> make_heap(LogicalDeviceAccessor device, OwnerRef layout, const Input& input);

        template<typename Input>
        HeapPipelineOwner::HeapPipelineOwner(LogicalDeviceAccessor device, OwnerRef layout, const Input& input)
            : device(device), layout(std::move(layout)) {
            if (!this->layout.use_count() || this->layout.get() != input.layout)
                throw std::invalid_argument("Pipeline root requires its runtime layout owner");
            require_device_owner(device);
            construct_pipeline(storage, *device.data_, input);
        }
        HeapPipelineOwner::~HeapPipelineOwner() {
            node.retire();
            destroy_pipeline(storage);
        }
        void HeapPipelineOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            const auto& value = *static_cast<const HeapPipelineOwner*>(owner);
            visitor(context, value.layout);
            visitor(context, value.device.owner_ref());
        }
        template<typename Input>
        void construct(void* storage, impl::LogicalDeviceImpl& device, const Input& input) {
            if (!device.device)
                throw std::invalid_argument("Pipeline requires a logical device owner");
            auto* slot = std::construct_at(static_cast<PipelineSlotStorage*>(storage));
            auto* pipeline = std::construct_at(reinterpret_cast<impl::PipelineImpl*>(slot->value));
            try {
                operation::initialize_pipeline(*pipeline, device, input);
            } catch (...) {
                std::destroy_at(pipeline);
                std::destroy_at(slot);
                throw;
            }
        }
        template<typename Input>
        std::shared_ptr<impl::PipelineImpl> make_heap(LogicalDeviceAccessor device, OwnerRef layout, const Input& input) {
            auto owner = std::make_shared<HeapPipelineOwner>(device, std::move(layout), input);
            auto* pipeline = &pipeline_at(owner->storage);
            owner->node.publish(owner, owner.get(), HeapPipelineOwner::VisitDependencies);
            pipeline->owner_node = &owner->node;
            return {std::move(owner), pipeline};
        }
    } // namespace
    void construct_pipeline(void* storage, LogicalDeviceAccessor device, const operation::GraphicsPipelineInput& input) {
        require_device_owner(device);
        construct_pipeline(storage, *device.data_, input);
    }
    void construct_pipeline(void* storage, impl::LogicalDeviceImpl& device, const operation::GraphicsPipelineInput& input) {
        construct(storage, device, input);
    }
    void construct_pipeline(void* storage, LogicalDeviceAccessor device, const operation::ComputePipelineInput& input) {
        require_device_owner(device);
        construct_pipeline(storage, *device.data_, input);
    }
    void construct_pipeline(void* storage, impl::LogicalDeviceImpl& device, const operation::ComputePipelineInput& input) {
        construct(storage, device, input);
    }
    impl::PipelineImpl& pipeline_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::PipelineImpl*>(static_cast<PipelineSlotStorage*>(storage)->value));
    }
    void destroy_pipeline(void* storage) noexcept {
        std::destroy_at(&pipeline_at(storage));
        std::destroy_at(static_cast<PipelineSlotStorage*>(storage));
    }
    std::shared_ptr<impl::PipelineImpl> make_heap_pipeline(LogicalDeviceAccessor device, OwnerRef layout, const operation::GraphicsPipelineInput& input) {
        return make_heap(device, std::move(layout), input);
    }
    std::shared_ptr<impl::PipelineImpl> make_heap_pipeline(LogicalDeviceAccessor device, OwnerRef layout, const operation::ComputePipelineInput& input) {
        return make_heap(device, std::move(layout), input);
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::PipelineImpl::~PipelineImpl() {
    operation::destroy_pipeline(*this);
}
