#include <VSL/utils/typed_resource_allocation.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    using Resources = t::ResourceSchema<t::DescriptorSet<2, t::UniformBinding<3, float, vsl::ShaderFlag::VERTEX>>>;
#if VSL_TYPED_CASE == 0
    using Schema = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3>>;
    constexpr t::ResourceAllocationPlan<Schema> PLAN(3);
    static_assert(PLAN.totals().buffers == 3 && PLAN.totals().payload_bytes == 12);
#elif VSL_TYPED_CASE == 1
    using Invalid = t::ResourceAllocationSchema<Resources>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 2
    using Invalid = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3>, t::BufferAllocation<2, 3>>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 3
    using Invalid = t::ResourceAllocationSchema<Resources, t::BufferAllocation<1, 3>>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 4
    using Invalid = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 4>>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 5
    using Images = t::ResourceSchema<t::DescriptorSet<2, t::TextureBinding<3, vsl::ShaderFlag::FRAGMENT>>>;
    using Invalid = t::ResourceAllocationSchema<Images, t::BufferAllocation<2, 3>>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 6
    using Invalid = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3, t::FixedElements<0>>>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 7
    using Invalid = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3, t::FixedElements<1>, static_cast<t::ResourceScope>(7)>>;
    static_assert(sizeof(Invalid) > 0);
#elif VSL_TYPED_CASE == 8
    using Schema = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3>>;
    void rejected(t::ResourceAllocationSizes<Schema>& sizes) {
        sizes.set<2, 3>(4);
    }
#elif VSL_TYPED_CASE == 9
    using Schema = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 3, t::RuntimeElements>>;
    void rejected(t::ResourceAllocationSizes<Schema>& sizes) {
        sizes.set<2, 4>(4);
    }
#elif VSL_TYPED_CASE == 10
    using Invalid = t::BufferAllocation<2, 3, int>;
    static_assert(sizeof(Invalid) > 0);
#endif
} // namespace
