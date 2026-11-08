#include <VSL/lifetime/frame_buffer_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_standard_layout_v<FrameBufferSlotStorage>);
    static_assert(offsetof(FrameBufferSlotStorage, value) == 0);
    static_assert(std::is_nothrow_default_constructible_v<FrameBufferOwnerContext>);
    static_assert(std::is_nothrow_default_constructible_v<impl::FrameBufferImpl>);
    namespace {
        struct HeapFrameBufferOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            std::vector<OwnerRef> dependencies;
            alignas(FrameBufferSlotStorage) std::byte storage[sizeof(FrameBufferSlotStorage)];
            HeapFrameBufferOwner(LogicalDeviceAccessor device, operation::FrameBufferInput input, OwnerRef pass, std::vector<OwnerRef> dependencies);
            ~HeapFrameBufferOwner();
            static void VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context);
        };
        HeapFrameBufferOwner::HeapFrameBufferOwner(LogicalDeviceAccessor device, operation::FrameBufferInput input, OwnerRef pass, std::vector<OwnerRef> dependencies)
            : device(device), dependencies(std::move(dependencies)) {
            if (!pass.use_count() || pass.get() != input.pass)
                throw std::invalid_argument("Framebuffer root requires its pass owner");
            detail::retain_owner(this->dependencies, pass);
            construct_frame_buffer(storage, device, std::move(input));
            try {
                set_frame_buffer_pass_owner(frame_buffer_at(storage), pass.alias(frame_buffer_at(storage).render_pass));
            } catch (...) {
                destroy_frame_buffer(storage);
                throw;
            }
        }
        HeapFrameBufferOwner::~HeapFrameBufferOwner() {
            node.retire();
            destroy_frame_buffer(storage);
        }
        void HeapFrameBufferOwner::VisitDependencies(const void* owner, DependencyNode::EdgeVisitor visitor, void* context) {
            const auto& value = *static_cast<const HeapFrameBufferOwner*>(owner);
            for (const auto& dependency : value.dependencies)
                visitor(context, dependency);
            visitor(context, OwnerRef(value.device.data_));
        }
    } // namespace
    void construct_frame_buffer(void* storage, LogicalDeviceAccessor device, operation::FrameBufferInput input) {
        if (!device.data_ || !device.data_.use_count())
            throw std::invalid_argument("Framebuffer requires a logical device owner");
        auto* slot = std::construct_at(static_cast<FrameBufferSlotStorage*>(storage));
        auto* context = std::construct_at(reinterpret_cast<FrameBufferOwnerContext*>(slot->context));
        auto* target = std::construct_at(reinterpret_cast<impl::FrameBufferImpl*>(slot->value));
        target->owner_context = context;
        try {
            operation::initialize_frame_buffer(*target, *device.data_, std::move(input));
        } catch (...) {
            std::destroy_at(target);
            std::destroy_at(context);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::FrameBufferImpl& frame_buffer_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::FrameBufferImpl*>(static_cast<FrameBufferSlotStorage*>(storage)->value));
    }
    void destroy_frame_buffer(void* storage) noexcept {
        auto* context = frame_buffer_at(storage).owner_context;
        std::destroy_at(&frame_buffer_at(storage));
        std::destroy_at(context);
        std::destroy_at(static_cast<FrameBufferSlotStorage*>(storage));
    }
    void set_frame_buffer_pass_owner(impl::FrameBufferImpl& target, OwnedRef<impl::RenderPassImpl> owner) {
        if (!target.owner_context || !owner.use_count() || owner.get() != target.render_pass)
            throw std::invalid_argument("Framebuffer getter requires the original pass owner");
        target.owner_context->pass = owner.weak();
    }
    std::shared_ptr<impl::FrameBufferImpl> make_heap_frame_buffer(LogicalDeviceAccessor device, operation::FrameBufferInput input,
                                                                  OwnerRef pass, std::vector<OwnerRef> dependencies) {
        auto owner = std::make_shared<HeapFrameBufferOwner>(device, std::move(input), std::move(pass), std::move(dependencies));
        auto* target = &frame_buffer_at(owner->storage);
        owner->node.publish(owner, owner.get(), HeapFrameBufferOwner::VisitDependencies);
        target->owner_node = &owner->node;
        return {std::move(owner), target};
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::FrameBufferImpl::~FrameBufferImpl() {
    operation::destroy_frame_buffer(*this);
}
