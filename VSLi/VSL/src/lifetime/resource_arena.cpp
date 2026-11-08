#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/shader_storage.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/render_pass_storage.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/storage/cpu_region.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct ArenaResource {
        typed::ResourcePlacement placement;
        BufferRef buffer;
        impl::ImageImpl* image = nullptr;
        impl::SamplerImpl* sampler = nullptr;
        impl::ResourceBindingLayoutImpl* layout = nullptr;
        impl::GraphicResourcePoolImpl* pool = nullptr;
        impl::GraphicResourceImpl* set = nullptr;
        impl::ShaderImpl* shader = nullptr;
        impl::PipelineLayoutImpl* pipeline_layout = nullptr;
        impl::RenderPassImpl* render_pass = nullptr;
        size_t buffer_offset = 0, buffer_bytes = 0;
        size_t owner_index = 0;
    };
    struct ResourceArenaOwner {
        LogicalDeviceAccessor device;
        std::shared_ptr<storage::detail::CPURegion> memory;
        typed::LifetimeGroup group;
        std::vector<ArenaResource> resources;
        std::vector<OwnerRef> dependencies;
        std::vector<size_t> constructed;
        DescriptorOwnerContext descriptor_context;
        bool claimed = false;
        ResourceArenaOwner(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                           std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                           std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment,
                           std::span<const LogicalUseState> owners, DependencyPolicy policy);
        ~ResourceArenaOwner();
        void preflight(std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers);
        void construct(size_t index, const ResourceInit& initializer);
        void publish(const std::shared_ptr<ResourceArenaOwner>& owner);
        void clear() noexcept;
        size_t retain_dependency(OwnerRef owner);
        static void VisitDependencies(const void* input, DependencyNode::EdgeVisitor visitor, void* context);
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
            case storage::ResourceKind::DESCRIPTOR_LAYOUT:
                return external ? std::holds_alternative<graphic_resource::BindingLayout>(value) : std::holds_alternative<DescriptorLayoutCreateInfo>(value);
            case storage::ResourceKind::DESCRIPTOR_POOL:
                return external ? std::holds_alternative<graphic_resource::Pool>(value) : std::holds_alternative<DescriptorPoolCreateInfo>(value);
            case storage::ResourceKind::DESCRIPTOR_SET:
                return !external && std::holds_alternative<DescriptorSetInit>(value);
            case storage::ResourceKind::SHADER:
                return external ? std::holds_alternative<ShaderAccessor>(value) : std::holds_alternative<ShaderCreateInfo>(value);
            case storage::ResourceKind::PIPELINE_LAYOUT:
                return external ? std::holds_alternative<PipelineLayoutAccessor>(value) : std::holds_alternative<PipelineLayoutInit>(value);
            case storage::ResourceKind::RENDER_PASS:
                return external ? std::holds_alternative<RenderPassAccessor>(value) : std::holds_alternative<RenderPassDescription>(value);
        }
        return false;
    }
    ResourceArenaOwner::ResourceArenaOwner(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                                           std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                                           std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment,
                                           std::span<const LogicalUseState> owners, DependencyPolicy policy)
        : device(device), memory(std::move(memory)), group(group) {
        if (!device.data_.use_count() || !device.data_ || !device.data_->device || !this->memory)
            throw std::invalid_argument("Resource arena requires a live device and CPU region");
        if (policy != DependencyPolicy::FIXED && policy != DependencyPolicy::DYNAMIC)
            throw std::invalid_argument("Unknown resource dependency policy");
        descriptor_context.policy = policy;
        preflight(placements, initializers);
        for (const auto& owner : owners)
            retain_dependency(owner.resource_use().state);
        constructed.reserve(resources.size());
        this->memory->claim(bytes, alignment);
        claimed = true;
        try {
            // Setは同じarena内のPool/Layoutを借用する。配置順と構築順は独立する。
            for (const bool sets : {false, true})
                for (size_t i = 0; i < resources.size(); ++i)
                    if (!resources[i].placement.external && (resources[i].placement.kind == storage::ResourceKind::DESCRIPTOR_SET || resources[i].placement.kind == storage::ResourceKind::PIPELINE_LAYOUT) == sets) {
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
                    resource.owner_index = retain_dependency(buffer->owner_ref());
                } else if (const auto* image = std::get_if<ImageAccessor>(&value)) {
                    if (!image->data_.use_count() || !image->data_ || !image->data_->image || !image->data_->view ||
                        !image->data_->use || image->data_->device != device.data_.get())
                        throw std::invalid_argument("External image requires an owner on the arena device");
                    (void)image->use_state();
                    resource.image = image->data_.get();
                    resource.owner_index = retain_dependency(owned_ref(image->data_, image->data_->owner_node));
                } else if (const auto* sampler = std::get_if<SamplerAccessor>(&value)) {
                    if (!sampler->data_.use_count() || !sampler->data_ || !sampler->data_->sampler || !sampler->data_->use ||
                        sampler->data_->device != device.data_.get())
                        throw std::invalid_argument("External sampler requires an owner on the arena device");
                    (void)sampler->use_state();
                    resource.sampler = sampler->data_.get();
                    resource.owner_index = retain_dependency(owned_ref(sampler->data_, sampler->data_->owner_node));
                } else if (const auto* layout = std::get_if<graphic_resource::BindingLayout>(&value)) {
                    if (!layout->data_.use_count() || !layout->data_ || !layout->data_->layout || layout->data_->device != device.data_.get())
                        throw std::invalid_argument("External descriptor layout requires an owner on the arena device");
                    resource.layout = layout->data_.get();
                    resource.owner_index = retain_dependency(owned_ref(layout->data_, layout->data_->owner_node));
                } else if (const auto* pool = std::get_if<graphic_resource::Pool>(&value)) {
                    if (!pool->data_.use_count() || !pool->data_ || !pool->data_->descriptor_pool || pool->data_->retiring ||
                        pool->data_->device != device.data_.get())
                        throw std::invalid_argument("External descriptor pool requires a live owner on the arena device");
                    const auto owner = pool->data_->owner.lock();
                    if (!owner.use_count() || pool->data_.owner_before(owner) || owner.owner_before(pool->data_))
                        throw std::invalid_argument("External descriptor pool requires its canonical owner");
                    resource.pool = pool->data_.get();
                    resource.owner_index = retain_dependency(owned_ref(owner, pool->data_->owner_node));
                } else if (const auto* pass = std::get_if<RenderPassAccessor>(&value)) {
                    (void)pass->get_device();
                    if (pass->data_->device != device.data_.get())
                        throw std::invalid_argument("External render pass requires the arena device");
                    resource.render_pass = pass->data_.get();
                    resource.owner_index = retain_dependency(pass->owner_ref());
                } else if (const auto* layout = std::get_if<PipelineLayoutAccessor>(&value)) {
                    (void)layout->get_device();
                    if (layout->data_->device != device.data_.get())
                        throw std::invalid_argument("External pipeline layout requires the arena device");
                    resource.pipeline_layout = layout->data_.get();
                    resource.owner_index = retain_dependency(layout->owner_ref());
                } else if (const auto* shader = std::get_if<ShaderAccessor>(&value)) {
                    if (!shader->data_.use_count() || !shader->data_ || !shader->data_->shader_module || shader->data_->device != device.data_.get())
                        throw std::invalid_argument("External shader requires an owner on the arena device");
                    resource.shader = shader->data_.get();
                    resource.owner_index = retain_dependency(shader->owner_ref());
                }
            }
            resources.push_back(std::move(resource));
        }
        for (const auto& value : initializers)
            if (const auto* set = std::get_if<DescriptorSetInit>(&value)) {
                (void)find(set->pool, storage::ResourceKind::DESCRIPTOR_POOL);
                (void)find(set->layout, storage::ResourceKind::DESCRIPTOR_LAYOUT);
            } else if (const auto* layout = std::get_if<PipelineLayoutInit>(&value)) {
                for (const auto& source : layout->layouts)
                    if (const auto* id = std::get_if<typed::ResourceId>(&source)) {
                        (void)find(*id, storage::ResourceKind::DESCRIPTOR_LAYOUT);
                    } else {
                        const auto& binding = std::get<graphic_resource::BindingLayout>(source);
                        if (!binding.data_ || !binding.data_.use_count() || !binding.data_->layout || binding.data_->device != device.data_.get())
                            throw std::invalid_argument("Pipeline layout source requires an owning layout on the arena device");
                    }
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
            case storage::ResourceKind::DESCRIPTOR_LAYOUT:
                construct_descriptor_layout(slot, device, std::get<DescriptorLayoutCreateInfo>(initializer));
                resource.layout = &descriptor_layout_at(slot);
                break;
            case storage::ResourceKind::DESCRIPTOR_POOL:
                construct_descriptor_pool(slot, device, std::get<DescriptorPoolCreateInfo>(initializer));
                resource.pool = &descriptor_pool_at(slot);
                break;
            case storage::ResourceKind::DESCRIPTOR_SET: {
                const auto& info = std::get<DescriptorSetInit>(initializer);
                auto* pool = find(info.pool, storage::ResourceKind::DESCRIPTOR_POOL).pool;
                auto* layout = find(info.layout, storage::ResourceKind::DESCRIPTOR_LAYOUT).layout;
                construct_descriptor_set(slot, *pool, *layout);
                resource.set = &descriptor_set_at(slot);
                break;
            }
            case storage::ResourceKind::RENDER_PASS:
                construct_render_pass(slot, device, std::get<RenderPassDescription>(initializer));
                resource.render_pass = &render_pass_at(slot);
                break;
            case storage::ResourceKind::PIPELINE_LAYOUT: {
                const auto& info = std::get<PipelineLayoutInit>(initializer);
                std::vector<impl::ResourceBindingLayoutImpl*> layouts;
                layouts.reserve(info.layouts.size());
                for (const auto& source : info.layouts)
                    if (const auto* id = std::get_if<typed::ResourceId>(&source))
                        layouts.push_back(find(*id, storage::ResourceKind::DESCRIPTOR_LAYOUT).layout);
                    else
                        layouts.push_back(std::get<graphic_resource::BindingLayout>(source).data_.get());
                construct_pipeline_layout(slot, device, layouts, info.push_ranges);
                resource.pipeline_layout = &pipeline_layout_at(slot);
                break;
            }
            case storage::ResourceKind::SHADER:
                construct_shader(slot, device, std::get<ShaderCreateInfo>(initializer));
                resource.shader = &shader_at(slot);
                break;
        }
    }
    void ResourceArenaOwner::publish(const std::shared_ptr<ResourceArenaOwner>& owner) {
        // 登録に必要な確保を全部終えてからweak aliasを公開する。
        for (const auto& resource : resources)
            if (resource.set) {
                size_t count = 0;
                auto* pool = resource.set->pool;
                std::erase_if(pool->resources, [](const auto& entry) { return entry.expired(); });
                for (const auto& other : resources)
                    if (other.set && other.set->pool == pool)
                        ++count;
                if (count > pool->resources.max_size() - pool->resources.size())
                    throw std::length_error("Descriptor resource registry capacity exceeded");
                pool->resources.reserve(pool->resources.size() + count);
            }
        descriptor_context.dependencies = dependencies;
        descriptor_context.node.publish(owner, this, VisitDependencies);
        for (auto& resource : resources) {
            if (!resource.placement.external) {
                auto* node = &descriptor_context.node;
                if (resource.buffer.data)
                    resource.buffer.data->owner_node = node;
                if (resource.image)
                    resource.image->owner_node = node;
                if (resource.sampler)
                    resource.sampler->owner_node = node;
                if (resource.layout)
                    resource.layout->owner_node = node;
                if (resource.pool)
                    resource.pool->owner_node = node;
                if (resource.set)
                    resource.set->owner_node = node;
                if (resource.shader)
                    resource.shader->owner_node = node;
                if (resource.pipeline_layout)
                    resource.pipeline_layout->owner_node = node;
                if (resource.render_pass)
                    resource.render_pass->owner_node = node;
            }
            if (resource.pool && !resource.placement.external)
                resource.pool->owner = owner;
            if (resource.set) {
                resource.set->owner_context = &descriptor_context;
                resource.set->pool->resources.emplace_back(std::shared_ptr<impl::GraphicResourceImpl>{owner, resource.set});
            }
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
                case storage::ResourceKind::DESCRIPTOR_LAYOUT:
                    destroy_descriptor_layout(slot);
                    break;
                case storage::ResourceKind::DESCRIPTOR_POOL:
                    destroy_descriptor_pool(slot);
                    break;
                case storage::ResourceKind::DESCRIPTOR_SET:
                    destroy_descriptor_set(slot);
                    break;
                case storage::ResourceKind::RENDER_PASS:
                    destroy_render_pass(slot);
                    break;
                case storage::ResourceKind::PIPELINE_LAYOUT:
                    destroy_pipeline_layout(slot);
                    break;
                case storage::ResourceKind::SHADER:
                    destroy_shader(slot);
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
        descriptor_context.node.retire();
        clear();
    }
    size_t ResourceArenaOwner::retain_dependency(OwnerRef owner) {
        if (!owner.use_count())
            throw std::invalid_argument("Resource arena dependency requires an owner");
        retain_owner(dependencies, owner);
        for (size_t i = 0; i < dependencies.size(); ++i)
            if (!dependencies[i].owner_before(owner) && !owner.owner_before(dependencies[i]))
                return i;
        throw std::logic_error("Resource arena dependency was not retained");
    }
    void ResourceArenaOwner::VisitDependencies(const void* input, DependencyNode::EdgeVisitor visitor, void* context) {
        const auto& root = *static_cast<const ResourceArenaOwner*>(input);
        for (const auto& owner : root.dependencies)
            visitor(context, owner);
        for (const auto& resource : root.resources)
            if (resource.set)
                visit_descriptor_edges(*resource.set, visitor, context);
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
                                 std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment,
                                 std::span<const LogicalUseState> dependencies, DependencyPolicy policy) {
        storage::detail::require_resource_layout(std::integral_constant<std::uint64_t, storage::RESOURCE_LAYOUT_ABI>{});
        _owner = std::make_shared<detail::ResourceArenaOwner>(device, group, placements, initializers, std::move(memory), bytes, alignment, dependencies, policy);
        _owner->publish(_owner);
    }
    std::shared_ptr<void> ResourceArena::owner() const {
        return _owner;
    }
    OwnerRef ResourceArena::owner_ref() const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        return _owner->descriptor_context.node.ref(_owner);
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
            return {_owner->dependencies.at(resource.owner_index).alias(resource.image).share()};
        return {{_owner, resource.image}};
    }
    RenderPassAccessor ResourceArena::render_pass(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::RENDER_PASS);
        return {{resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.render_pass}};
    }
    PipelineLayoutAccessor ResourceArena::pipeline_layout(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::PIPELINE_LAYOUT);
        return PipelineLayoutAccessor({resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.pipeline_layout});
    }
    ShaderAccessor ResourceArena::shader(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::SHADER);
        return {{resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.shader}};
    }
    SamplerAccessor ResourceArena::sampler(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::SAMPLER);
        if (resource.placement.external)
            return {_owner->dependencies.at(resource.owner_index).alias(resource.sampler).share()};
        return {{_owner, resource.sampler}};
    }
    graphic_resource::BindingLayout ResourceArena::descriptor_layout(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::DESCRIPTOR_LAYOUT);
        return graphic_resource::BindingLayout(std::shared_ptr<impl::ResourceBindingLayoutImpl>{
            resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.layout});
    }
    graphic_resource::Pool ResourceArena::descriptor_pool(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::DESCRIPTOR_POOL);
        return {{resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.pool}};
    }
    GraphicResource ResourceArena::descriptor_set(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::DESCRIPTOR_SET);
        return {{_owner, resource.set}};
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
