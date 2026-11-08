#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/render_pass_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::RenderPassImpl>);
    namespace {
        struct HeapRenderPassOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(RenderPassSlotStorage) std::byte storage[sizeof(RenderPassSlotStorage)];
            HeapRenderPassOwner(LogicalDeviceAccessor device, RenderPassDescription description);
            ~HeapRenderPassOwner();
        };
        HeapRenderPassOwner::HeapRenderPassOwner(LogicalDeviceAccessor device, RenderPassDescription description) : device(device) {
            require_device_owner(device);
            construct_render_pass(storage, *device.data_, std::move(description));
        }
        HeapRenderPassOwner::~HeapRenderPassOwner() {
            node.retire();
            destroy_render_pass(storage);
        }
    } // namespace
    void construct_render_pass(void* storage, LogicalDeviceAccessor device, RenderPassDescription description) {
        require_device_owner(device);
        construct_render_pass(storage, *device.data_, std::move(description));
    }
    void construct_render_pass(void* storage, impl::LogicalDeviceImpl& device, RenderPassDescription description) {
        if (!device.device)
            throw std::invalid_argument("Render pass requires a device owner");
        auto* slot = std::construct_at(static_cast<RenderPassSlotStorage*>(storage));
        auto* pass = std::construct_at(reinterpret_cast<impl::RenderPassImpl*>(slot->value));
        try {
            operation::initialize_render_pass(*pass, device, std::move(description));
        } catch (...) {
            std::destroy_at(pass);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::RenderPassImpl& render_pass_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::RenderPassImpl*>(static_cast<RenderPassSlotStorage*>(storage)->value));
    }
    void destroy_render_pass(void* storage) noexcept {
        std::destroy_at(&render_pass_at(storage));
        std::destroy_at(static_cast<RenderPassSlotStorage*>(storage));
    }
    std::shared_ptr<impl::RenderPassImpl> make_heap_render_pass(LogicalDeviceAccessor device, RenderPassDescription description) {
        auto owner = std::make_shared<HeapRenderPassOwner>(device, std::move(description));
        auto* pass = &render_pass_at(owner->storage);
        return detail::publish_device_leaf(std::move(owner), pass);
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::RenderPassImpl::~RenderPassImpl() {
    operation::destroy_render_pass(*this);
}
