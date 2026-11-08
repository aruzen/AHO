#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/shader_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/exceptions.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::ShaderImpl>);
    namespace {
        struct HeapShaderOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(ShaderSlotStorage) std::byte storage[sizeof(ShaderSlotStorage)];
            HeapShaderOwner(LogicalDeviceAccessor device, ShaderCreateInfo info);
            ~HeapShaderOwner();
        };
        HeapShaderOwner::HeapShaderOwner(LogicalDeviceAccessor device, ShaderCreateInfo info) : device(device) {
            require_device_owner(device);
            construct_shader(storage, *device.data_, std::move(info));
        }
        HeapShaderOwner::~HeapShaderOwner() {
            node.retire();
            destroy_shader(storage);
        }
    } // namespace
    void construct_shader(void* storage, LogicalDeviceAccessor device, ShaderCreateInfo info) {
        require_device_owner(device);
        construct_shader(storage, *device.data_, std::move(info));
    }
    void construct_shader(void* storage, impl::LogicalDeviceImpl& device, ShaderCreateInfo info) {
        if (!device.device)
            throw exceptions::RuntimeException("Shader", "A logical device owner is required");
        auto* slot = std::construct_at(static_cast<ShaderSlotStorage*>(storage));
        auto* shader = std::construct_at(reinterpret_cast<impl::ShaderImpl*>(slot->shader));
        try {
            operation::initialize_shader(*shader, device, std::move(info));
        } catch (...) {
            std::destroy_at(shader);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::ShaderImpl& shader_at(void* storage) {
        return *std::launder(reinterpret_cast<impl::ShaderImpl*>(static_cast<ShaderSlotStorage*>(storage)->shader));
    }
    void destroy_shader(void* storage) noexcept {
        std::destroy_at(&shader_at(storage));
        std::destroy_at(static_cast<ShaderSlotStorage*>(storage));
    }
    std::shared_ptr<impl::ShaderImpl> make_heap_shader(LogicalDeviceAccessor device, ShaderCreateInfo info) {
        if (!device.data_ || !device.data_->device)
            throw exceptions::RuntimeException("Shader", "A live logical device is required");
        auto owner = std::make_shared<HeapShaderOwner>(device, std::move(info));
        auto* shader = &shader_at(owner->storage);
        return detail::publish_device_leaf(std::move(owner), shader);
    }
} // namespace VSL_NAMESPACE::lifetime

VSL_NAMESPACE::impl::ShaderImpl::~ShaderImpl() {
    operation::destroy_shader(*this);
}
