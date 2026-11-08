#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <new>

namespace VSL_NAMESPACE::lifetime {
    static_assert(std::is_nothrow_default_constructible_v<impl::SamplerImpl>);
    namespace {
        struct HeapSamplerOwner {
            DependencyNode node;
            LogicalDeviceAccessor device;
            alignas(SamplerSlotStorage) std::byte storage[sizeof(SamplerSlotStorage)];
            HeapSamplerOwner(LogicalDeviceAccessor device, const SamplerCreateInfo& info);
            ~HeapSamplerOwner();
        };
        HeapSamplerOwner::HeapSamplerOwner(LogicalDeviceAccessor device, const SamplerCreateInfo& info) : device(device) {
            construct_sampler(storage, device, info);
        }
        HeapSamplerOwner::~HeapSamplerOwner() {
            node.retire();
            destroy_sampler(storage);
        }
    } // namespace

    void construct_sampler(void* storage, LogicalDeviceAccessor device, const SamplerCreateInfo& info) {
        if (!device.data_ || !device.data_->device)
            throw std::invalid_argument("Sampler requires a live device");
        auto* slot = std::construct_at(static_cast<SamplerSlotStorage*>(storage));
        auto* sampler = std::construct_at(reinterpret_cast<impl::SamplerImpl*>(slot->sampler));
        sampler->use = std::construct_at(reinterpret_cast<ResourceUseState*>(slot->use));
        try {
            operation::initialize_sampler(*sampler, *device.data_, info);
        } catch (...) {
            std::destroy_at(sampler->use);
            std::destroy_at(sampler);
            std::destroy_at(slot);
            throw;
        }
    }
    impl::SamplerImpl& sampler_at(void* storage) {
        auto* slot = static_cast<SamplerSlotStorage*>(storage);
        return *std::launder(reinterpret_cast<impl::SamplerImpl*>(slot->sampler));
    }
    void destroy_sampler(void* storage) noexcept {
        auto& sampler = sampler_at(storage);
        operation::destroy_sampler(sampler);
        std::destroy_at(sampler.use);
        std::destroy_at(&sampler);
        std::destroy_at(static_cast<SamplerSlotStorage*>(storage));
    }
    std::shared_ptr<impl::SamplerImpl> make_heap_sampler(LogicalDeviceAccessor device, const SamplerCreateInfo& info) {
        auto owner = std::make_shared<HeapSamplerOwner>(device, info);
        auto* sampler = &sampler_at(owner->storage);
        return detail::publish_leaf(std::move(owner), sampler);
    }
} // namespace VSL_NAMESPACE::lifetime
