#include <VSL/storage/resource_layout.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/image_storage.hpp>
#include <VSL/lifetime/sampler_storage.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/shader_storage.hpp>
#include <VSL/lifetime/pipeline_layout_storage.hpp>
#include <VSL/lifetime/render_pass_storage.hpp>
#include <VSL/lifetime/pipeline_storage.hpp>

namespace VSL_NAMESPACE::storage {
    static_assert(sizeof(impl::PipelineImpl) == ImplLayout<impl::PipelineImpl>::SIZE);
    static_assert(alignof(impl::PipelineImpl) == ImplLayout<impl::PipelineImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::PipelineSlotStorage) == ResourceSlotLayout<impl::PipelineImpl>::SIZE);
    static_assert(alignof(lifetime::PipelineSlotStorage) == ResourceSlotLayout<impl::PipelineImpl>::ALIGNMENT);
    static_assert(ResourceSlotLayout<impl::PipelineImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(sizeof(impl::RenderPassImpl) == ImplLayout<impl::RenderPassImpl>::SIZE);
    static_assert(alignof(impl::RenderPassImpl) == ImplLayout<impl::RenderPassImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::RenderPassSlotStorage) == ResourceSlotLayout<impl::RenderPassImpl>::SIZE);
    static_assert(alignof(lifetime::RenderPassSlotStorage) == ResourceSlotLayout<impl::RenderPassImpl>::ALIGNMENT);
    static_assert(ResourceSlotLayout<impl::RenderPassImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(sizeof(impl::PipelineLayoutImpl) == ImplLayout<impl::PipelineLayoutImpl>::SIZE);
    static_assert(alignof(impl::PipelineLayoutImpl) == ImplLayout<impl::PipelineLayoutImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::PipelineLayoutSlotStorage) == ResourceSlotLayout<impl::PipelineLayoutImpl>::SIZE);
    static_assert(alignof(lifetime::PipelineLayoutSlotStorage) == ResourceSlotLayout<impl::PipelineLayoutImpl>::ALIGNMENT);
    static_assert(ResourceSlotLayout<impl::PipelineLayoutImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(sizeof(impl::ShaderImpl) == ImplLayout<impl::ShaderImpl>::SIZE);
    static_assert(alignof(impl::ShaderImpl) == ImplLayout<impl::ShaderImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::ShaderSlotStorage) == ResourceSlotLayout<impl::ShaderImpl>::SIZE);
    static_assert(alignof(lifetime::ShaderSlotStorage) == ResourceSlotLayout<impl::ShaderImpl>::ALIGNMENT);
    static_assert(ResourceSlotLayout<impl::ShaderImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(sizeof(impl::BufferImpl) == ImplLayout<impl::BufferImpl>::SIZE);
    static_assert(alignof(impl::BufferImpl) == ImplLayout<impl::BufferImpl>::ALIGNMENT);
    static_assert(sizeof(impl::ImageImpl) == ImplLayout<impl::ImageImpl>::SIZE);
    static_assert(alignof(impl::ImageImpl) == ImplLayout<impl::ImageImpl>::ALIGNMENT);
    static_assert(sizeof(impl::SamplerImpl) == ImplLayout<impl::SamplerImpl>::SIZE);
    static_assert(alignof(impl::SamplerImpl) == ImplLayout<impl::SamplerImpl>::ALIGNMENT);
    static_assert(sizeof(impl::ResourceBindingLayoutImpl) == ImplLayout<impl::ResourceBindingLayoutImpl>::SIZE);
    static_assert(alignof(impl::ResourceBindingLayoutImpl) == ImplLayout<impl::ResourceBindingLayoutImpl>::ALIGNMENT);
    static_assert(sizeof(impl::GraphicResourcePoolImpl) == ImplLayout<impl::GraphicResourcePoolImpl>::SIZE);
    static_assert(alignof(impl::GraphicResourcePoolImpl) == ImplLayout<impl::GraphicResourcePoolImpl>::ALIGNMENT);
    static_assert(sizeof(impl::GraphicResourceImpl) == ImplLayout<impl::GraphicResourceImpl>::SIZE);
    static_assert(alignof(impl::GraphicResourceImpl) == ImplLayout<impl::GraphicResourceImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::DescriptorLayoutSlotStorage) == ResourceSlotLayout<impl::ResourceBindingLayoutImpl>::SIZE);
    static_assert(alignof(lifetime::DescriptorLayoutSlotStorage) == ResourceSlotLayout<impl::ResourceBindingLayoutImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::DescriptorPoolSlotStorage) == ResourceSlotLayout<impl::GraphicResourcePoolImpl>::SIZE);
    static_assert(alignof(lifetime::DescriptorPoolSlotStorage) == ResourceSlotLayout<impl::GraphicResourcePoolImpl>::ALIGNMENT);
    static_assert(sizeof(lifetime::DescriptorSetSlotStorage) == ResourceSlotLayout<impl::GraphicResourceImpl>::SIZE);
    static_assert(alignof(lifetime::DescriptorSetSlotStorage) == ResourceSlotLayout<impl::GraphicResourceImpl>::ALIGNMENT);
    static_assert(ResourceSlotLayout<impl::ResourceBindingLayoutImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(ResourceSlotLayout<impl::GraphicResourcePoolImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(ResourceSlotLayout<impl::GraphicResourceImpl>::USE_OFFSET == NO_SEPARATE_USE);
    static_assert(sizeof(lifetime::BufferSlotStorage) == ResourceSlotLayout<impl::BufferImpl>::SIZE);
    static_assert(alignof(lifetime::BufferSlotStorage) == ResourceSlotLayout<impl::BufferImpl>::ALIGNMENT);
    static_assert(offsetof(lifetime::BufferSlotStorage, use) == ResourceSlotLayout<impl::BufferImpl>::USE_OFFSET);
    static_assert(offsetof(lifetime::BufferSlotStorage, buffer) == 0);
    static_assert(sizeof(lifetime::ImageSlotStorage) == ResourceSlotLayout<impl::ImageImpl>::SIZE);
    static_assert(alignof(lifetime::ImageSlotStorage) == ResourceSlotLayout<impl::ImageImpl>::ALIGNMENT);
    static_assert(offsetof(lifetime::ImageSlotStorage, use) == ResourceSlotLayout<impl::ImageImpl>::USE_OFFSET);
    static_assert(offsetof(lifetime::ImageSlotStorage, image) == 0);
    static_assert(sizeof(lifetime::SamplerSlotStorage) == ResourceSlotLayout<impl::SamplerImpl>::SIZE);
    static_assert(alignof(lifetime::SamplerSlotStorage) == ResourceSlotLayout<impl::SamplerImpl>::ALIGNMENT);
    static_assert(offsetof(lifetime::SamplerSlotStorage, use) == ResourceSlotLayout<impl::SamplerImpl>::USE_OFFSET);
    static_assert(offsetof(lifetime::SamplerSlotStorage, sampler) == 0);
    static_assert(BufferLayout::STRIDE == ResourceSlotLayout<impl::BufferImpl>::SIZE);
    namespace detail {
        void require_resource_layout(std::integral_constant<std::uint64_t, RESOURCE_LAYOUT_ABI>) {}
    } // namespace detail
} // namespace VSL_NAMESPACE::storage
