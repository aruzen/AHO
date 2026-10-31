#include <VSL/lifetime/buffer_storage.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::BufferImpl>);
    static_assert(std::is_nothrow_default_constructible_v<ResourceUseState>);
    namespace {
        struct HeapBufferOwner {
            LogicalDeviceAccessor device;
            alignas(BufferSlotStorage) std::byte storage[sizeof(BufferSlotStorage)];
            HeapBufferOwner(LogicalDeviceAccessor device, const BufferCreateInfo& info);
            ~HeapBufferOwner();
        };
        HeapBufferOwner::HeapBufferOwner(LogicalDeviceAccessor device, const BufferCreateInfo& info) : device(device) {
            construct_buffer(storage, device, info);
        }
        HeapBufferOwner::~HeapBufferOwner() {
            destroy_buffer(storage);
        }
    } // namespace

    void construct_buffer(void* storage, LogicalDeviceAccessor device, const BufferCreateInfo& info) {
        if (!device.data_ || !device.data_->device)
            throw std::invalid_argument("Buffer requires a live device");
        auto* slot = std::construct_at(static_cast<BufferSlotStorage*>(storage));
        auto* buffer = std::construct_at(reinterpret_cast<impl::BufferImpl*>(slot->buffer));
        auto* use = std::construct_at(reinterpret_cast<ResourceUseState*>(slot->use));
        buffer->use = use;
        try {
            backend::initialize_buffer(*buffer, *device.data_, info);
        } catch (...) {
            std::destroy_at(use);
            std::destroy_at(buffer);
            std::destroy_at(slot);
            throw;
        }
    }
    BufferRef buffer_at(void* storage) {
        auto* slot = static_cast<BufferSlotStorage*>(storage);
        return {std::launder(reinterpret_cast<impl::BufferImpl*>(slot->buffer))};
    }
    void destroy_buffer(void* storage) noexcept {
        auto ref = buffer_at(storage);
        auto* slot = static_cast<BufferSlotStorage*>(storage);
        backend::destroy_buffer(*ref.data);
        std::destroy_at(ref.data->use);
        std::destroy_at(ref.data);
        std::destroy_at(slot);
    }
    std::shared_ptr<impl::BufferImpl> make_heap_buffer(LogicalDeviceAccessor device, const BufferCreateInfo& info) {
        auto owner = std::make_shared<HeapBufferOwner>(device, info);
        auto* buffer = buffer_at(owner->storage).data;
        return {std::move(owner), buffer};
    }
} // namespace VSL_NAMESPACE::lifetime
