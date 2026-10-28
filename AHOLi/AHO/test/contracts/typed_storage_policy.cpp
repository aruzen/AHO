#include <AHO/resource/typed_storage_policy.hpp>
#include <VSL/utils/typed_resource_bundle.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace t = vsl::typed;
    namespace r = aho::resource;
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using Allocation = t::ResourceAllocationSchema<t::ResourceSchema<Set>, t::BufferAllocation<0, 0>>;
    using Policy = r::TypedStoragePolicy<>;
    using Bundle = t::TypedResourceBundle<Allocation, Policy>;
    static_assert(std::same_as<decltype(std::declval<Bundle::Frame&>().buffer<0, 0>().backend()), r::Buffer<r::BufferType::GPU_FRIENDLY>&>);
    [[maybe_unused]] void contract(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const vsl::ComputePhase& phase) {
#if VSL_TYPED_CASE == 1
        Bundle bundle(manager, descriptors);
        bundle.update(phase, [](Bundle::Frame& frame) {
            const float value = 1;
            frame.buffer<0, 0>().copy(std::span(&value, 1));
        });
#elif VSL_TYPED_CASE == 2
        using Bad = r::TypedResourceBuffer<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER, r::Buffer<r::BufferType::ON_GPU>>;
        static_assert(sizeof(Bad) > 0);
#elif VSL_TYPED_CASE == 3
        using Native = vsl::Buffer<vsl::MemoryType::TRANSFER_SOURCE, vsl::MemoryProperty::HOST_VISIBLE>;
        using Source = r::Buffer<r::BufferType::CUSTOM, Native>;
        using Bad = r::TypedResourceBuffer<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER, Source>;
        static_assert(sizeof(Bad) > 0);
#else
        Bundle first(manager, descriptors);
        Bundle second(Policy{}, manager, descriptors, {});
        first.update(phase, [](Bundle::Frame& frame) {
            const std::uint32_t value = 7;
            frame.buffer<0, 0>().copy(std::span(&value, 1));
        });
#endif
    }
} // namespace
