#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/storage/cpu_region.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct ArenaResource {
        typed::ResourcePlacement placement;
        BufferRef buffer;
        impl::ImageImpl* image = nullptr;
        impl::SamplerImpl* sampler = nullptr;
        size_t buffer_offset = 0, buffer_bytes = 0;
        size_t owner_index = 0;
    };
    struct ResourceArenaOwner {
        LogicalDeviceAccessor device;
        std::shared_ptr<storage::detail::CPURegion> memory;
        typed::LifetimeGroup group;
        std::vector<ArenaResource> resources;
        std::vector<std::shared_ptr<void>> dependencies;
        std::vector<size_t> constructed;
        bool claimed = false;
        ResourceArenaOwner(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                           std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                           std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment);
        ~ResourceArenaOwner();
        void preflight(std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers);
        void construct(size_t index, const ResourceInit& initializer);
        void clear() noexcept;
        size_t retain_dependency(const std::shared_ptr<void>& owner);
        const ArenaResource& find(typed::ResourceId id, storage::ResourceKind kind) const;
    };
    bool initializer_matches(const ResourceInit& value, storage::ResourceKind kind, bool external);

    bool initializer_matches(const ResourceInit& value, storage::ResourceKind kind, bool external) {
        switch (kind) {
            case storage::ResourceKind::BUFFER:
                return external ? std::holds_alternative<BufferView>(value) : std::holds_alternative<BufferCreateInfo>(value);
            case storage::ResourceKind::IMAGE:
                return external ? std::holds_alternative<ImageAccessor>(value) : std::holds_alternative<ImageCreateInfo>(value);
            case storage::ResourceKind::SAMPLER:
                return external ? std::holds_alternative<SamplerAccessor>(value) : std::holds_alternative<SamplerCreateInfo>(value);
        }
        return false;
    }
    ResourceArenaOwner::ResourceArenaOwner(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                                           std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                                           std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment)
        : device(device), memory(std::move(memory)), group(group) {
        if (!device.data_ || !device.data_->device || !this->memory)
            throw std::invalid_argument("Resource arena requires a live device and CPU region");
        preflight(placements, initializers);
        constructed.reserve(resources.size());
        this->memory->claim(bytes, alignment);
        claimed = true;
        try {
            for (size_t i = 0; i < resources.size(); ++i)
                if (!resources[i].placement.external) {
                    construct(i, initializers[i]);
                    constructed.push_back(i);
                }
        } catch (...) {
            clear();
            throw;
        }
    }
    void ResourceArenaOwner::preflight(std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers) {
        if (placements.size() != initializers.size())
            throw std::invalid_argument("Resource initializers must match every plan entry, including external resources");
        resources.reserve(placements.size());
        for (size_t i = 0; i < placements.size(); ++i) {
            auto resource = ArenaResource{placements[i]};
            const auto& value = initializers[i];
            if (!initializer_matches(value, resource.placement.kind, resource.placement.external))
                throw std::invalid_argument("Resource initializer kind or ownership does not match its plan entry");
            if (resource.placement.external) {
                if (const auto* buffer = std::get_if<BufferView>(&value)) {
                    const auto* ref = buffer->ref().data;
                    if (!buffer->owner().use_count() || !ref || !ref->buffer || !ref->use || ref->device != device.data_.get())
                        throw std::invalid_argument("External buffer requires an owner on the arena device");
                    resource.buffer = buffer->ref();
                    resource.buffer_offset = buffer->byte_offset();
                    resource.buffer_bytes = buffer->size();
                    resource.owner_index = retain_dependency(buffer->owner());
                } else if (const auto* image = std::get_if<ImageAccessor>(&value)) {
                    if (!image->data_.use_count() || !image->data_ || !image->data_->image || !image->data_->view ||
                        !image->data_->use || image->data_->device != device.data_.get())
                        throw std::invalid_argument("External image requires an owner on the arena device");
                    (void)image->use_state();
                    resource.image = image->data_.get();
                    resource.owner_index = retain_dependency(image->data_);
                } else {
                    const auto& sampler = std::get<SamplerAccessor>(value);
                    if (!sampler.data_.use_count() || !sampler.data_ || !sampler.data_->sampler || !sampler.data_->use ||
                        sampler.data_->device != device.data_.get())
                        throw std::invalid_argument("External sampler requires an owner on the arena device");
                    (void)sampler.use_state();
                    resource.sampler = sampler.data_.get();
                    resource.owner_index = retain_dependency(sampler.data_);
                }
            }
            resources.push_back(std::move(resource));
        }
    }
    void ResourceArenaOwner::construct(size_t index, const ResourceInit& initializer) {
        auto& resource = resources[index];
        auto* slot = memory->base + resource.placement.offset;
        switch (resource.placement.kind) {
            case storage::ResourceKind::BUFFER:
                construct_buffer(slot, device, std::get<BufferCreateInfo>(initializer));
                resource.buffer = buffer_at(slot);
                resource.buffer_bytes = resource.buffer.size();
                break;
            case storage::ResourceKind::IMAGE:
                construct_image(slot, device, std::get<ImageCreateInfo>(initializer));
                resource.image = &image_at(slot);
                break;
            case storage::ResourceKind::SAMPLER:
                construct_sampler(slot, device, std::get<SamplerCreateInfo>(initializer));
                resource.sampler = &sampler_at(slot);
                break;
        }
    }
    void ResourceArenaOwner::clear() noexcept {
        while (!constructed.empty()) {
            const auto& placement = resources[constructed.back()].placement;
            auto* slot = memory->base + placement.offset;
            switch (placement.kind) {
                case storage::ResourceKind::BUFFER:
                    destroy_buffer(slot);
                    break;
                case storage::ResourceKind::IMAGE:
                    destroy_image(slot);
                    break;
                case storage::ResourceKind::SAMPLER:
                    destroy_sampler(slot);
                    break;
            }
            constructed.pop_back();
        }
        if (claimed) {
            memory->release();
            claimed = false;
        }
    }
    ResourceArenaOwner::~ResourceArenaOwner() {
        clear();
    }
    size_t ResourceArenaOwner::retain_dependency(const std::shared_ptr<void>& owner) {
        for (size_t i = 0; i < dependencies.size(); ++i)
            if (!dependencies[i].owner_before(owner) && !owner.owner_before(dependencies[i]))
                return i;
        dependencies.push_back(owner);
        return dependencies.size() - 1;
    }
    const ArenaResource& ResourceArenaOwner::find(typed::ResourceId id, storage::ResourceKind kind) const {
        for (const auto& resource : resources)
            if (resource.placement.resource == id) {
                if (resource.placement.kind != kind)
                    throw std::invalid_argument("Resource arena view kind mismatch");
                return resource;
            }
        throw std::out_of_range("Unknown resource arena ID");
    }
} // namespace VSL_NAMESPACE::lifetime::detail

namespace VSL_NAMESPACE::lifetime {
    ResourceArena::ResourceArena(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                                 std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                                 std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment) {
        storage::detail::require_resource_layout(std::integral_constant<std::uint64_t, storage::RESOURCE_LAYOUT_ABI>{});
        _owner = std::make_shared<detail::ResourceArenaOwner>(device, group, placements, initializers, std::move(memory), bytes, alignment);
    }
    std::shared_ptr<void> ResourceArena::owner() const {
        return _owner;
    }
    size_t ResourceArena::size() const {
        return _owner ? _owner->resources.size() : 0;
    }
    size_t ResourceArena::slots() const {
        return _owner ? _owner->constructed.size() : 0;
    }
    typed::LifetimeGroup ResourceArena::lifetime() const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        return _owner->group;
    }
    BufferView ResourceArena::buffer(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::BUFFER);
        if (resource.placement.external)
            return {resource.buffer, _owner->dependencies.at(resource.owner_index), resource.buffer_offset, resource.buffer_bytes};
        return {resource.buffer, _owner, resource.buffer_offset, resource.buffer_bytes};
    }
    ImageAccessor ResourceArena::image(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::IMAGE);
        if (resource.placement.external)
            return {{_owner->dependencies.at(resource.owner_index), resource.image}};
        return {{_owner, resource.image}};
    }
    SamplerAccessor ResourceArena::sampler(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::SAMPLER);
        if (resource.placement.external)
            return {{_owner->dependencies.at(resource.owner_index), resource.sampler}};
        return {{_owner, resource.sampler}};
    }
    BufferRef ResourceArena::borrow_buffer(typed::ResourceId id) const& {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::BUFFER);
        if (resource.buffer_offset || resource.buffer_bytes != resource.buffer.size())
            throw std::logic_error("Partial external buffers require an owning range view");
        return resource.buffer;
    }
} // namespace VSL_NAMESPACE::lifetime
