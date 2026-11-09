#pragma once
#include <VSL/lifetime/resource_plan.hpp>
#include <VSL/lifetime/buffer_view.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <variant>
#include <type_traits>

namespace VSL_NAMESPACE::lifetime::detail {
    // kindはplacementで検証し、解決後の参照はどれか一つだけを保存する。
    struct ArenaResource {
        using Reference = std::variant<impl::BufferImpl*, impl::ImageImpl*, impl::SamplerImpl*,
                                       impl::ResourceBindingLayoutImpl*, impl::GraphicResourcePoolImpl*, impl::GraphicResourceImpl*,
                                       impl::ShaderImpl*, impl::PipelineLayoutImpl*, impl::RenderPassImpl*, impl::PipelineImpl*,
                                       impl::FrameBufferImpl*, impl::CommandPoolImpl*, impl::CommandBufferImpl*, impl::SemaphoreImpl*, impl::FenceImpl*>;
        typed::ResourcePlacement placement;
        Reference reference;
        size_t buffer_offset = 0, buffer_bytes = 0;
        size_t owner_index = 0;
        template<typename Impl>
        Impl* get() const noexcept;
        BufferRef buffer() const { return BufferAccess::Ref(get<impl::BufferImpl>()); }
    };
    template<typename Impl>
    Impl* ArenaResource::get() const noexcept {
        const auto* value = std::get_if<Impl*>(&reference);
        return value ? *value : nullptr;
    }
    static_assert(sizeof(ArenaResource::Reference) <= 2 * sizeof(void*));
    static_assert(std::is_nothrow_copy_assignable_v<ArenaResource::Reference>);
} // namespace VSL_NAMESPACE::lifetime::detail
