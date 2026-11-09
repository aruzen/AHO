#include <VSL/operation/graphics_pipeline_state_access.hpp>
#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/descriptor_access.hpp>
#include <VSL/lifetime/shader_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/sampler_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/arena_resource.hpp>
#include <VSL/lifetime/device_owner.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/shader_storage.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/render_pass_storage.hpp>
#include <VSL/lifetime/pipeline_storage.hpp>
#include <VSL/lifetime/frame_buffer_storage.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/storage/cpu_region.hpp>

namespace {
    using VSL_NAMESPACE::operation::detail::GraphicsPipelineStateAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::SynchronizationAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ShaderAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::ImageAccess;
    using VSL_NAMESPACE::lifetime::detail::SamplerAccess;
} // namespace

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace VSL_NAMESPACE::lifetime::detail {
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
        void preflight(const RuntimeLayoutSource& source);
        void preflight(const PipelinePassSource& source);
        void preflight(const PipelineShaderSource& source);
        void preflight(const FrameBufferInit& input);
        void preflight(const CommandBufferInit& input);
        impl::PipelineLayoutImpl* resolve(const RuntimeLayoutSource& source) const;
        impl::RenderPassImpl* resolve(const PipelinePassSource& source) const;
        impl::ShaderImpl* resolve(const PipelineShaderSource& source) const;
        impl::ImageImpl* resolve(const FrameBufferImageSource& source) const;
        impl::CommandPoolImpl* resolve(const CommandPoolSource& source) const;
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
            case storage::ResourceKind::PIPELINE:
                return external ? std::holds_alternative<PipelineAccessor>(value) : std::holds_alternative<GraphicsPipelineInit>(value) || std::holds_alternative<ComputePipelineInit>(value);
            case storage::ResourceKind::FRAME_BUFFER:
                return external ? std::holds_alternative<FrameBufferAccessor>(value) : std::holds_alternative<FrameBufferInit>(value);
            case storage::ResourceKind::COMMAND_POOL:
                return external ? std::holds_alternative<CommandPool>(value) : std::holds_alternative<CommandPoolCreateInfo>(value);
            case storage::ResourceKind::COMMAND_BUFFER:
                return external ? std::holds_alternative<CommandBuffer>(value) : std::holds_alternative<CommandBufferInit>(value);
            case storage::ResourceKind::SEMAPHORE:
                return external ? std::holds_alternative<SemaphoreHolder>(value) : std::holds_alternative<SemaphoreCreateInfo>(value);
            case storage::ResourceKind::FENCE:
                return external ? std::holds_alternative<FenceHolder>(value) : std::holds_alternative<FenceCreateInfo>(value);
        }
        return false;
    }
    ResourceArenaOwner::ResourceArenaOwner(LogicalDeviceAccessor device, typed::LifetimeGroup group,
                                           std::span<const typed::ResourcePlacement> placements, std::span<const ResourceInit> initializers,
                                           std::shared_ptr<storage::detail::CPURegion> memory, size_t bytes, size_t alignment,
                                           std::span<const LogicalUseState> owners, DependencyPolicy policy)
        : device(device), memory(std::move(memory)), group(group) {
        require_device_owner(device);
        if (!device.data_.use_count() || !device.data_ || !device.data_->device || !this->memory)
            throw std::invalid_argument("Resource arena requires a live device and CPU region");
        if (policy != DependencyPolicy::FIXED && policy != DependencyPolicy::DYNAMIC)
            throw std::invalid_argument("Unknown resource dependency policy");
        descriptor_context.policy = policy;
        descriptor_context.device_parent = device.data_->owner_node;
        preflight(placements, initializers);
        for (const auto& owner : owners)
            retain_dependency(owner.resource_use().state);
        constructed.reserve(resources.size());
        this->memory->claim(bytes, alignment);
        claimed = true;
        try {
            // 配置順と独立して、借用先を先に構築する。
            const auto stage = [](storage::ResourceKind kind) {
                if (kind == storage::ResourceKind::PIPELINE || kind == storage::ResourceKind::FRAME_BUFFER)
                    return 2;
                if (kind == storage::ResourceKind::DESCRIPTOR_SET || kind == storage::ResourceKind::PIPELINE_LAYOUT || kind == storage::ResourceKind::COMMAND_BUFFER)
                    return 1;
                return 0;
            };
            for (int phase = 0; phase < 3; ++phase)
                for (size_t i = 0; i < resources.size(); ++i)
                    if (!resources[i].placement.external && stage(resources[i].placement.kind) == phase) {
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
                    const auto* ref = detail::BufferAccess::Impl(buffer->ref());
                    if (!buffer->owner().use_count() || !ref || !ref->buffer || !ref->use || ref->device != device.data_.get())
                        throw std::invalid_argument("External buffer requires an owner on the arena device");
                    resource.reference = detail::BufferAccess::Impl(buffer->ref());
                    resource.buffer_offset = buffer->byte_offset();
                    resource.buffer_bytes = buffer->size();
                    resource.owner_index = retain_dependency(buffer->owner_ref());
                } else if (const auto* image = std::get_if<ImageAccessor>(&value)) {
                    if (!ImageAccess::Owned(*(image)).use_count() || !ImageAccess::Owned(*(image)) || !ImageAccess::Owned(*(image))->image ||
                        !ImageAccess::Owned(*(image))->use || ImageAccess::Owned(*(image))->device != device.data_.get())
                        throw std::invalid_argument("External image requires an owner on the arena device");
                    (void)image->use_state();
                    resource.reference = ImageAccess::Owned(*(image)).get();
                    resource.owner_index = retain_dependency(ImageAccess::Owned(*(image)));
                } else if (const auto* sampler = std::get_if<SamplerAccessor>(&value)) {
                    if (!SamplerAccess::Owned(*(sampler)).use_count() || !SamplerAccess::Owned(*(sampler)) || !SamplerAccess::Owned(*(sampler))->sampler || !SamplerAccess::Owned(*(sampler))->use ||
                        SamplerAccess::Owned(*(sampler))->device != device.data_.get())
                        throw std::invalid_argument("External sampler requires an owner on the arena device");
                    (void)sampler->use_state();
                    resource.reference = SamplerAccess::Owned(*(sampler)).get();
                    resource.owner_index = retain_dependency(SamplerAccess::Owned(*(sampler)));
                } else if (const auto* layout = std::get_if<graphic_resource::BindingLayout>(&value)) {
                    if (!DescriptorAccess::Owned(*(layout)).use_count() || !DescriptorAccess::Owned(*(layout)) || !DescriptorAccess::Owned(*(layout))->layout || DescriptorAccess::Owned(*(layout))->device != device.data_.get())
                        throw std::invalid_argument("External descriptor layout requires an owner on the arena device");
                    resource.reference = DescriptorAccess::Owned(*(layout)).get();
                    resource.owner_index = retain_dependency(DescriptorAccess::Owned(*(layout)));
                } else if (const auto* pool = std::get_if<graphic_resource::Pool>(&value)) {
                    if (!DescriptorAccess::Owned(*(pool)).use_count() || !DescriptorAccess::Owned(*(pool)) || !DescriptorAccess::Owned(*(pool))->descriptor_pool || DescriptorAccess::Owned(*(pool))->retiring ||
                        DescriptorAccess::Owned(*(pool))->device != device.data_.get())
                        throw std::invalid_argument("External descriptor pool requires a live owner on the arena device");
                    const auto owner = DescriptorAccess::Owned(*(pool))->owner.lock();
                    if (!owner.use_count() || DescriptorAccess::Owned(*(pool)).owner_before(owner) || owner.owner_before(OwnerAccess::Shared(DescriptorAccess::Owned(*(pool)))))
                        throw std::invalid_argument("External descriptor pool requires its canonical owner");
                    resource.reference = DescriptorAccess::Owned(*(pool)).get();
                    resource.owner_index = retain_dependency(owned_ref(owner, DescriptorAccess::Owned(*(pool))->owner_node));
                } else if (const auto* semaphore = std::get_if<SemaphoreHolder>(&value)) {
                    if (semaphore->get_device().data_ != device.data_)
                        throw std::invalid_argument("External semaphore requires the arena device");
                    resource.reference = SynchronizationAccess::Owned(*(semaphore)).get();
                    resource.owner_index = retain_dependency(semaphore->owner_ref());
                } else if (const auto* fence = std::get_if<FenceHolder>(&value)) {
                    if (fence->get_device().data_ != device.data_)
                        throw std::invalid_argument("External fence requires the arena device");
                    resource.reference = SynchronizationAccess::Owned(*(fence)).get();
                    resource.owner_index = retain_dependency(fence->owner_ref());
                } else if (const auto* command_pool = std::get_if<CommandPool>(&value)) {
                    if (command_pool->get_device().data_ != device.data_)
                        throw std::invalid_argument("External command pool requires the arena device");
                    resource.reference = CommandAccess::Data(*(command_pool)).get();
                    resource.owner_index = retain_dependency(command_pool->owner_ref());
                } else if (const auto* command_buffer = std::get_if<CommandBuffer>(&value)) {
                    if (command_buffer->get_pool().get_device().data_ != device.data_)
                        throw std::invalid_argument("External command ring requires the arena device");
                    resource.reference = CommandAccess::Data(*(command_buffer)).get();
                    resource.owner_index = retain_dependency(command_buffer->owner_ref());
                } else if (const auto* frame_buffer = std::get_if<FrameBufferAccessor>(&value)) {
                    if (frame_buffer->get_device().data_ != device.data_)
                        throw std::invalid_argument("External framebuffer requires the arena device");
                    resource.reference = GraphicsAccess::Owned(*(frame_buffer)).get();
                    resource.owner_index = retain_dependency(frame_buffer->owner_ref());
                } else if (const auto* pipeline = std::get_if<PipelineAccessor>(&value)) {
                    (void)pipeline->get_device();
                    if (GraphicsAccess::Owned(*(pipeline))->device != device.data_.get())
                        throw std::invalid_argument("External pipeline requires the arena device");
                    resource.reference = GraphicsAccess::Owned(*(pipeline)).get();
                    resource.owner_index = retain_dependency(pipeline->owner_ref());
                } else if (const auto* pass = std::get_if<RenderPassAccessor>(&value)) {
                    (void)pass->get_device();
                    if (GraphicsAccess::Owned(*(pass))->device != device.data_.get())
                        throw std::invalid_argument("External render pass requires the arena device");
                    resource.reference = GraphicsAccess::Owned(*(pass)).get();
                    resource.owner_index = retain_dependency(pass->owner_ref());
                } else if (const auto* layout = std::get_if<PipelineLayoutAccessor>(&value)) {
                    (void)layout->get_device();
                    if (GraphicsAccess::Owned(*(layout))->device != device.data_.get())
                        throw std::invalid_argument("External pipeline layout requires the arena device");
                    resource.reference = GraphicsAccess::Owned(*(layout)).get();
                    resource.owner_index = retain_dependency(layout->owner_ref());
                } else if (const auto* shader = std::get_if<ShaderAccessor>(&value)) {
                    if (!ShaderAccess::Owned(*(shader)).use_count() || !ShaderAccess::Owned(*(shader)) || !ShaderAccess::Owned(*(shader))->shader_module || ShaderAccess::Owned(*(shader))->device != device.data_.get())
                        throw std::invalid_argument("External shader requires an owner on the arena device");
                    resource.reference = ShaderAccess::Owned(*(shader)).get();
                    resource.owner_index = retain_dependency(shader->owner_ref());
                }
            }
            resources.push_back(std::move(resource));
        }
        for (const auto& value : initializers)
            if (const auto* set = std::get_if<DescriptorSetInit>(&value)) {
                (void)find(set->pool, storage::ResourceKind::DESCRIPTOR_POOL);
                (void)find(set->layout, storage::ResourceKind::DESCRIPTOR_LAYOUT);
            } else if (const auto* framebuffer = std::get_if<FrameBufferInit>(&value)) {
                preflight(*framebuffer);
            } else if (const auto* command_buffer = std::get_if<CommandBufferInit>(&value)) {
                preflight(*command_buffer);
            } else if (const auto* graphics = std::get_if<GraphicsPipelineInit>(&value)) {
                preflight(graphics->layout);
                preflight(graphics->pass);
                for (const auto& shader : graphics->shaders)
                    preflight(shader);
            } else if (const auto* compute = std::get_if<ComputePipelineInit>(&value)) {
                preflight(compute->layout);
                preflight(compute->shader);
            } else if (const auto* layout = std::get_if<PipelineLayoutInit>(&value)) {
                for (const auto& source : layout->layouts)
                    if (const auto* id = std::get_if<typed::ResourceId>(&source)) {
                        (void)find(*id, storage::ResourceKind::DESCRIPTOR_LAYOUT);
                    } else {
                        const auto& binding = std::get<graphic_resource::BindingLayout>(source);
                        if (!DescriptorAccess::Owned(binding) || !DescriptorAccess::Owned(binding).use_count() || !DescriptorAccess::Owned(binding)->layout || DescriptorAccess::Owned(binding)->device != device.data_.get())
                            throw std::invalid_argument("Pipeline layout source requires an owning layout on the arena device");
                    }
            }
    }
    void ResourceArenaOwner::preflight(const RuntimeLayoutSource& source) {
        if (const auto* id = std::get_if<typed::ResourceId>(&source)) {
            (void)find(*id, storage::ResourceKind::PIPELINE_LAYOUT);
        } else {
            const auto& layout = std::get<PipelineLayoutAccessor>(source);
            if (layout.get_device().data_ != device.data_)
                throw std::invalid_argument("Pipeline runtime layout requires the arena device");
            retain_dependency(layout.owner_ref());
        }
    }
    void ResourceArenaOwner::preflight(const CommandBufferInit& input) {
        if (const auto* id = std::get_if<typed::ResourceId>(&input.pool)) {
            (void)find(*id, storage::ResourceKind::COMMAND_POOL);
        } else {
            const auto& pool = std::get<CommandPool>(input.pool);
            if (pool.get_device().data_ != device.data_)
                throw std::invalid_argument("Command ring source requires the arena device");
            retain_dependency(pool.owner_ref());
        }
    }
    impl::CommandPoolImpl* ResourceArenaOwner::resolve(const CommandPoolSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return find(*id, storage::ResourceKind::COMMAND_POOL).get<impl::CommandPoolImpl>();
        return CommandAccess::Data(std::get<CommandPool>(source)).get();
    }
    void ResourceArenaOwner::preflight(const PipelinePassSource& source) {
        if (const auto* id = std::get_if<typed::ResourceId>(&source)) {
            (void)find(*id, storage::ResourceKind::RENDER_PASS);
        } else if (std::get<RenderPassAccessor>(source).get_device().data_ != device.data_) {
            throw std::invalid_argument("Pipeline pass source requires the arena device");
        }
    }
    void ResourceArenaOwner::preflight(const PipelineShaderSource& source) {
        if (const auto* id = std::get_if<typed::ResourceId>(&source)) {
            (void)find(*id, storage::ResourceKind::SHADER);
        } else {
            const auto& shader = std::get<ShaderAccessor>(source);
            if (!ShaderAccess::Owned(shader) || !ShaderAccess::Owned(shader).use_count() || !ShaderAccess::Owned(shader)->shader_module || ShaderAccess::Owned(shader)->device != device.data_.get())
                throw std::invalid_argument("Pipeline shader source requires an owner on the arena device");
        }
    }
    void ResourceArenaOwner::preflight(const FrameBufferInit& input) {
        preflight(input.pass);
        if (const auto* pass = std::get_if<RenderPassAccessor>(&input.pass))
            retain_dependency(pass->owner_ref());
        for (const auto& row : input.attachments)
            for (const auto& source : row) {
                if (const auto* id = std::get_if<typed::ResourceId>(&source)) {
                    (void)find(*id, storage::ResourceKind::IMAGE);
                } else {
                    const auto& image = std::get<ImageAccessor>(source);
                    if (!ImageAccess::Owned(image) || !ImageAccess::Owned(image).use_count() || !ImageAccess::Owned(image)->image || !ImageAccess::Owned(image)->view ||
                        !ImageAccess::Owned(image)->use || ImageAccess::Owned(image)->device != device.data_.get())
                        throw std::invalid_argument("Framebuffer image requires an owner on the arena device");
                    retain_dependency(ImageAccess::Owned(image));
                }
            }
    }
    impl::ImageImpl* ResourceArenaOwner::resolve(const FrameBufferImageSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return find(*id, storage::ResourceKind::IMAGE).get<impl::ImageImpl>();
        return ImageAccess::Owned(std::get<ImageAccessor>(source)).get();
    }
    impl::PipelineLayoutImpl* ResourceArenaOwner::resolve(const RuntimeLayoutSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return find(*id, storage::ResourceKind::PIPELINE_LAYOUT).get<impl::PipelineLayoutImpl>();
        return GraphicsAccess::Owned(std::get<PipelineLayoutAccessor>(source)).get();
    }
    impl::RenderPassImpl* ResourceArenaOwner::resolve(const PipelinePassSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return find(*id, storage::ResourceKind::RENDER_PASS).get<impl::RenderPassImpl>();
        return GraphicsAccess::Owned(std::get<RenderPassAccessor>(source)).get();
    }
    impl::ShaderImpl* ResourceArenaOwner::resolve(const PipelineShaderSource& source) const {
        if (const auto* id = std::get_if<typed::ResourceId>(&source))
            return find(*id, storage::ResourceKind::SHADER).get<impl::ShaderImpl>();
        return ShaderAccess::Owned(std::get<ShaderAccessor>(source)).get();
    }
    void ResourceArenaOwner::construct(size_t index, const ResourceInit& initializer) {
        auto& resource = resources[index];
        auto* slot = memory->base + resource.placement.offset;
        switch (resource.placement.kind) {
            case storage::ResourceKind::COMMAND_POOL:
                construct_command_pool(slot, *device.data_, std::get<CommandPoolCreateInfo>(initializer));
                resource.reference = &command_pool_at(slot);
                break;
            case storage::ResourceKind::SEMAPHORE:
                construct_semaphore(slot, *device.data_, std::get<SemaphoreCreateInfo>(initializer));
                resource.reference = &semaphore_at(slot);
                break;
            case storage::ResourceKind::FENCE:
                construct_fence(slot, *device.data_, std::get<FenceCreateInfo>(initializer));
                resource.reference = &fence_at(slot);
                break;
            case storage::ResourceKind::COMMAND_BUFFER: {
                const auto& info = std::get<CommandBufferInit>(initializer);
                auto* pool = resolve(info.pool);
                WeakOwnedRef<impl::CommandPoolImpl> pool_owner;
                if (const auto* id = std::get_if<typed::ResourceId>(&info.pool)) {
                    const auto& source = find(*id, storage::ResourceKind::COMMAND_POOL);
                    if (source.placement.external)
                        pool_owner = dependencies.at(source.owner_index).alias(pool).weak();
                } else {
                    pool_owner = std::get<CommandPool>(info.pool).owner_ref().alias(pool).weak();
                }
                construct_command_buffer(slot, *pool, info.count);
                resource.reference = &command_buffer_at(slot);
                resource.get<impl::CommandBufferImpl>()->owner_context->pool = std::move(pool_owner);
                break;
            }
            case storage::ResourceKind::BUFFER:
                construct_buffer(slot, *device.data_, std::get<BufferCreateInfo>(initializer));
                resource.reference = detail::BufferAccess::Impl(buffer_at(slot));
                resource.buffer_bytes = resource.buffer().size();
                break;
            case storage::ResourceKind::IMAGE:
                construct_image(slot, *device.data_, std::get<ImageCreateInfo>(initializer));
                resource.reference = &image_at(slot);
                break;
            case storage::ResourceKind::SAMPLER:
                construct_sampler(slot, *device.data_, std::get<SamplerCreateInfo>(initializer));
                resource.reference = &sampler_at(slot);
                break;
            case storage::ResourceKind::DESCRIPTOR_LAYOUT:
                construct_descriptor_layout(slot, *device.data_, std::get<DescriptorLayoutCreateInfo>(initializer));
                resource.reference = &descriptor_layout_at(slot);
                break;
            case storage::ResourceKind::DESCRIPTOR_POOL:
                construct_descriptor_pool(slot, *device.data_, std::get<DescriptorPoolCreateInfo>(initializer));
                resource.reference = &descriptor_pool_at(slot);
                break;
            case storage::ResourceKind::DESCRIPTOR_SET: {
                const auto& info = std::get<DescriptorSetInit>(initializer);
                auto* pool = find(info.pool, storage::ResourceKind::DESCRIPTOR_POOL).get<impl::GraphicResourcePoolImpl>();
                auto* layout = find(info.layout, storage::ResourceKind::DESCRIPTOR_LAYOUT).get<impl::ResourceBindingLayoutImpl>();
                construct_descriptor_set(slot, *pool, *layout);
                resource.reference = &descriptor_set_at(slot);
                break;
            }
            case storage::ResourceKind::RENDER_PASS:
                construct_render_pass(slot, *device.data_, std::get<RenderPassDescription>(initializer));
                resource.reference = &render_pass_at(slot);
                break;
            case storage::ResourceKind::PIPELINE:
                if (const auto* graphics = std::get_if<GraphicsPipelineInit>(&initializer)) {
                    std::vector<impl::ShaderImpl*> shaders;
                    shaders.reserve(graphics->shaders.size());
                    for (const auto& source : graphics->shaders)
                        shaders.push_back(resolve(source));
                    const operation::GraphicsPipelineInput input{resolve(graphics->layout), resolve(graphics->pass),
                                                                 &GraphicsPipelineStateAccess::Read(graphics->state), shaders, graphics->subpass};
                    construct_pipeline(slot, *device.data_, input);
                } else {
                    const auto& compute = std::get<ComputePipelineInit>(initializer);
                    const operation::ComputePipelineInput input{resolve(compute.layout), resolve(compute.shader)};
                    construct_pipeline(slot, *device.data_, input);
                }
                resource.reference = &pipeline_at(slot);
                break;
            case storage::ResourceKind::FRAME_BUFFER: {
                const auto& info = std::get<FrameBufferInit>(initializer);
                operation::FrameBufferInput input{resolve(info.pass), info.width, info.height, info.layers, {}};
                input.attachments.reserve(info.attachments.size());
                for (const auto& row : info.attachments) {
                    std::vector<impl::ImageImpl*> images;
                    images.reserve(row.size());
                    for (const auto& source : row)
                        images.push_back(resolve(source));
                    input.attachments.push_back(std::move(images));
                }
                WeakOwnedRef<impl::RenderPassImpl> pass_owner;
                if (const auto* id = std::get_if<typed::ResourceId>(&info.pass)) {
                    const auto& pass = find(*id, storage::ResourceKind::RENDER_PASS);
                    if (pass.placement.external)
                        pass_owner = dependencies.at(pass.owner_index).alias(input.pass).weak();
                } else {
                    pass_owner = std::get<RenderPassAccessor>(info.pass).owner_ref().alias(input.pass).weak();
                }
                construct_frame_buffer(slot, *device.data_, std::move(input));
                resource.reference = &frame_buffer_at(slot);
                resource.get<impl::FrameBufferImpl>()->owner_context->pass = std::move(pass_owner);
                break;
            }
            case storage::ResourceKind::PIPELINE_LAYOUT: {
                const auto& info = std::get<PipelineLayoutInit>(initializer);
                std::vector<impl::ResourceBindingLayoutImpl*> layouts;
                layouts.reserve(info.layouts.size());
                for (const auto& source : info.layouts)
                    if (const auto* id = std::get_if<typed::ResourceId>(&source))
                        layouts.push_back(find(*id, storage::ResourceKind::DESCRIPTOR_LAYOUT).get<impl::ResourceBindingLayoutImpl>());
                    else
                        layouts.push_back(DescriptorAccess::Owned(std::get<graphic_resource::BindingLayout>(source)).get());
                construct_pipeline_layout(slot, *device.data_, layouts, info.push_ranges);
                resource.reference = &pipeline_layout_at(slot);
                break;
            }
            case storage::ResourceKind::SHADER:
                construct_shader(slot, *device.data_, std::get<ShaderCreateInfo>(initializer));
                resource.reference = &shader_at(slot);
                break;
        }
    }
    void ResourceArenaOwner::publish(const std::shared_ptr<ResourceArenaOwner>& owner) {
        // 登録に必要な確保を全部終えてからweak aliasを公開する。
        for (const auto& resource : resources)
            if (resource.get<impl::GraphicResourceImpl>()) {
                size_t count = 0;
                auto* pool = resource.get<impl::GraphicResourceImpl>()->pool;
                std::erase_if(pool->resources, [](const auto& entry) { return entry.expired(); });
                for (const auto& other : resources)
                    if (other.get<impl::GraphicResourceImpl>() && other.get<impl::GraphicResourceImpl>()->pool == pool)
                        ++count;
                if (count > pool->resources.max_size() - pool->resources.size())
                    throw std::length_error("Descriptor resource registry capacity exceeded");
                pool->resources.reserve(pool->resources.size() + count);
            }
        descriptor_context.dependencies = dependencies;
        descriptor_context.dependency_storage = &dependencies;
        descriptor_context.node.publish(owner, this, VisitDependencies);
        for (auto& resource : resources) {
            if (!resource.placement.external) {
                auto* node = &descriptor_context.node;
                std::visit([node](auto* value) { value->owner_node = node; }, resource.reference);
            }
            if (resource.get<impl::GraphicResourcePoolImpl>() && !resource.placement.external)
                resource.get<impl::GraphicResourcePoolImpl>()->owner = owner;
            if (resource.get<impl::GraphicResourceImpl>()) {
                resource.get<impl::GraphicResourceImpl>()->owner_context = &descriptor_context;
                resource.get<impl::GraphicResourceImpl>()->pool->resources.emplace_back(std::shared_ptr<impl::GraphicResourceImpl>{owner, resource.get<impl::GraphicResourceImpl>()});
            }
        }
        for (const auto& resource : resources)
            if (!resource.placement.external && resource.get<impl::FrameBufferImpl>() && !resource.get<impl::FrameBufferImpl>()->owner_context->pass.use_count()) {
                auto* pass = resource.get<impl::FrameBufferImpl>()->render_pass;
                if (pass->owner_node != &descriptor_context.node)
                    throw std::logic_error("Framebuffer pass owner was not retained");
                set_frame_buffer_pass_owner(*resource.get<impl::FrameBufferImpl>(), descriptor_context.node.ref(owner).alias(pass));
            }
        for (const auto& resource : resources)
            if (!resource.placement.external && resource.get<impl::CommandBufferImpl>() && !resource.get<impl::CommandBufferImpl>()->owner_context->pool.use_count()) {
                auto* pool = resource.get<impl::CommandBufferImpl>()->command_pool;
                if (pool->owner_node != &descriptor_context.node)
                    throw std::logic_error("Command ring pool owner was not retained");
                set_command_buffer_pool_owner(*resource.get<impl::CommandBufferImpl>(), descriptor_context.node.ref(owner).alias(pool));
            }
    }
    void ResourceArenaOwner::clear() noexcept {
        while (!constructed.empty()) {
            const auto& placement = resources[constructed.back()].placement;
            auto* slot = memory->base + placement.offset;
            switch (placement.kind) {
                case storage::ResourceKind::SEMAPHORE:
                    destroy_semaphore(slot);
                    break;
                case storage::ResourceKind::FENCE:
                    destroy_fence(slot);
                    break;
                case storage::ResourceKind::COMMAND_POOL:
                    destroy_command_pool(slot);
                    break;
                case storage::ResourceKind::COMMAND_BUFFER:
                    destroy_command_buffer(slot);
                    break;
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
                case storage::ResourceKind::PIPELINE:
                    destroy_pipeline(slot);
                    break;
                case storage::ResourceKind::FRAME_BUFFER:
                    destroy_frame_buffer(slot);
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
        visitor(context, root.device.owner_ref());
        for (const auto& owner : root.dependencies)
            visitor(context, owner);
        for (const auto& resource : root.resources)
            if (resource.get<impl::GraphicResourceImpl>())
                visit_descriptor_edges(*resource.get<impl::GraphicResourceImpl>(), visitor, context);
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
    void ResourceArena::freeze_dependencies() const {
        const auto owner = _owner;
        if (!owner)
            throw std::logic_error("Resource arena is empty");
        std::vector<impl::GraphicResourceImpl*> sets;
        for (const auto& resource : owner->resources)
            if (auto* set = resource.get<impl::GraphicResourceImpl>())
                sets.push_back(set);
        freeze_descriptor_dependencies(owner->descriptor_context, sets);
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
            return detail::BufferAccess::View(resource.buffer(), _owner->dependencies.at(resource.owner_index), resource.buffer_offset, resource.buffer_bytes);
        return detail::BufferAccess::View(resource.buffer(), _owner, resource.buffer_offset, resource.buffer_bytes);
    }
    ImageAccessor ResourceArena::image(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::IMAGE);
        if (resource.placement.external)
            return ImageAccess::FromOwned(_owner->dependencies.at(resource.owner_index).alias(resource.get<impl::ImageImpl>()));
        return ImageAccess::FromOwned(_owner->descriptor_context.node.ref(_owner).alias(resource.get<impl::ImageImpl>()));
    }
    RenderPassAccessor ResourceArena::render_pass(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::RENDER_PASS);
        const auto owner = resource.placement.external ? _owner->dependencies.at(resource.owner_index) : _owner->descriptor_context.node.ref(_owner);
        return GraphicsAccess::FromOwned(owner.alias(resource.get<impl::RenderPassImpl>()));
    }
    PipelineAccessor ResourceArena::pipeline(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::PIPELINE);
        const auto owner = resource.placement.external ? _owner->dependencies.at(resource.owner_index) : _owner->descriptor_context.node.ref(_owner);
        return GraphicsAccess::FromOwned(owner.alias(resource.get<impl::PipelineImpl>()));
    }
    FrameBufferAccessor ResourceArena::frame_buffer(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::FRAME_BUFFER);
        const auto owner = resource.placement.external ? _owner->dependencies.at(resource.owner_index) : _owner->descriptor_context.node.ref(_owner);
        return GraphicsAccess::FromOwned(owner.alias(resource.get<impl::FrameBufferImpl>()));
    }
    CommandPool ResourceArena::command_pool(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::COMMAND_POOL);
        return {CommandAccess::Pool({resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.get<impl::CommandPoolImpl>()})};
    }
    SemaphoreHolder ResourceArena::semaphore(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::SEMAPHORE);
        const auto owner = resource.placement.external ? _owner->dependencies.at(resource.owner_index) : _owner->descriptor_context.node.ref(_owner);
        return SynchronizationAccess::FromOwned(owner.alias(resource.get<impl::SemaphoreImpl>()));
    }
    FenceHolder ResourceArena::fence(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::FENCE);
        const auto owner = resource.placement.external ? _owner->dependencies.at(resource.owner_index) : _owner->descriptor_context.node.ref(_owner);
        return SynchronizationAccess::FromOwned(owner.alias(resource.get<impl::FenceImpl>()));
    }
    CommandBuffer ResourceArena::command_buffer(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::COMMAND_BUFFER);
        return CommandAccess::Attach({resource.placement.external ? _owner->dependencies.at(resource.owner_index).share() : _owner, resource.get<impl::CommandBufferImpl>()});
    }
    PipelineLayoutAccessor ResourceArena::pipeline_layout(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::PIPELINE_LAYOUT);
        const auto owner = resource.placement.external ? _owner->dependencies.at(resource.owner_index) : _owner->descriptor_context.node.ref(_owner);
        return GraphicsAccess::FromOwned(owner.alias(resource.get<impl::PipelineLayoutImpl>()));
    }
    ShaderAccessor ResourceArena::shader(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::SHADER);
        if (resource.placement.external)
            return ShaderAccess::FromOwned(_owner->dependencies.at(resource.owner_index).alias(resource.get<impl::ShaderImpl>()));
        return ShaderAccess::FromOwned(_owner->descriptor_context.node.ref(_owner).alias(resource.get<impl::ShaderImpl>()));
    }
    SamplerAccessor ResourceArena::sampler(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::SAMPLER);
        if (resource.placement.external)
            return SamplerAccess::FromOwned(_owner->dependencies.at(resource.owner_index).alias(resource.get<impl::SamplerImpl>()));
        return SamplerAccess::FromOwned(_owner->descriptor_context.node.ref(_owner).alias(resource.get<impl::SamplerImpl>()));
    }
    graphic_resource::BindingLayout ResourceArena::descriptor_layout(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::DESCRIPTOR_LAYOUT);
        if (resource.placement.external)
            return DescriptorAccess::FromOwned(_owner->dependencies.at(resource.owner_index).alias(resource.get<impl::ResourceBindingLayoutImpl>()));
        return DescriptorAccess::FromOwned(_owner->descriptor_context.node.ref(_owner).alias(resource.get<impl::ResourceBindingLayoutImpl>()));
    }
    graphic_resource::Pool ResourceArena::descriptor_pool(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::DESCRIPTOR_POOL);
        if (resource.placement.external)
            return DescriptorAccess::FromOwned(_owner->dependencies.at(resource.owner_index).alias(resource.get<impl::GraphicResourcePoolImpl>()));
        return DescriptorAccess::FromOwned(_owner->descriptor_context.node.ref(_owner).alias(resource.get<impl::GraphicResourcePoolImpl>()));
    }
    GraphicResource ResourceArena::descriptor_set(typed::ResourceId id) const {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::DESCRIPTOR_SET);
        return DescriptorAccess::FromOwned(_owner->descriptor_context.node.ref(_owner).alias(resource.get<impl::GraphicResourceImpl>()));
    }
    BufferRef ResourceArena::borrow_buffer(typed::ResourceId id) const& {
        if (!_owner)
            throw std::logic_error("Resource arena is empty");
        const auto& resource = _owner->find(id, storage::ResourceKind::BUFFER);
        if (resource.buffer_offset || resource.buffer_bytes != resource.buffer().size())
            throw std::logic_error("Partial external buffers require an owning range view");
        return resource.buffer();
    }
} // namespace VSL_NAMESPACE::lifetime
