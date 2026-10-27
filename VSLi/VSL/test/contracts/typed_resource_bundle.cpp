#include <VSL/utils/typed_resource_bundle.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    using Resources = t::ResourceSchema<t::DescriptorSet<0, t::UniformBinding<0, float, vsl::ShaderFlag::VERTEX>, t::UniformBinding<1, std::uint32_t, vsl::ShaderFlag::VERTEX>>>;
    using Allocation = t::ResourceAllocationSchema<Resources, t::BufferAllocation<0, 0>, t::BufferAllocation<0, 1, t::FixedElements<1>, t::ResourceScope::SHARED>>;
    using Bundle = t::TypedResourceBundle<Allocation>;
    using Textures = t::ResourceSchema<t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT, 2>>>;
    using External = t::ResourceAllocationSchema<Textures, t::ExternalAllocation<1, 2>>;
#if VSL_TYPED_CASE == 0
    static_assert(!std::is_copy_constructible_v<Bundle> && std::is_nothrow_move_constructible_v<Bundle>);
    static_assert(!std::is_copy_constructible_v<Bundle::Frame>);
    void accepted(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const vsl::ComputePhase& phase, const t::TypedTexture& texture) {
        Bundle bundle(manager, descriptors);
        bundle.initialize_shared([](Bundle::Shared& shared) {
            const std::uint32_t value = 7;
            shared.buffer<0, 1>().copy(std::span(&value, 1));
        });
        bundle.update(phase, [](Bundle::Frame& frame) {
            const float value = 1;
            frame.buffer<0, 0>().copy(std::span(&value, 1));
        });
        bundle.get(phase).set<0>().require_complete();
        t::TypedResourceBundle<External> external(manager, descriptors, t::external_resource<1, 2>(std::array{texture, texture}));
    }
#elif VSL_TYPED_CASE == 1
    void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const t::TypedSampler& sampler) {
        t::TypedResourceBundle<External> bundle(manager, descriptors, t::external_resource<1, 2>(std::array{sampler, sampler}));
    }
#elif VSL_TYPED_CASE == 2
    using One = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::UniformBinding<0, float, vsl::ShaderFlag::VERTEX>>>, t::ExternalAllocation<0, 0>>;
    void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const t::TypedUniformBuffer<std::uint32_t>& buffer) {
        t::TypedResourceBundle<One> bundle(manager, descriptors, t::external_resource<0, 0>(buffer));
    }
#elif VSL_TYPED_CASE == 3
    void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const t::TypedTexture& texture) {
        t::TypedResourceBundle<External> bundle(manager, descriptors, t::external_resource<1, 2>(texture));
    }
#elif VSL_TYPED_CASE == 4
    void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors) {
        t::TypedResourceBundle<External> bundle(manager, descriptors);
    }
#elif VSL_TYPED_CASE == 5
    void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const t::TypedUniformBuffer<float>& buffer) {
        Bundle bundle(manager, descriptors, t::external_resource<0, 0>(buffer));
    }
#elif VSL_TYPED_CASE == 6
    void rejected(Bundle::Frame& frame, std::span<const std::uint32_t> values) {
        frame.buffer<0, 1>().copy(values);
    }
#elif VSL_TYPED_CASE == 7
    void rejected(const Bundle::Frame& frame, std::span<const float> values) {
        frame.buffer<0, 0>().copy(values);
    }
#elif VSL_TYPED_CASE == 8
    void rejected(Bundle::Frame& frame) {
        frame.buffer<0, 0, 1>();
    }
#elif VSL_TYPED_CASE == 9
    void rejected(Bundle::Frame& frame) {
        frame.buffer<0, 2>();
    }
#elif VSL_TYPED_CASE == 10
    void rejected(Bundle::Shared& shared) {
        shared.buffer<0, 0>();
    }
#elif VSL_TYPED_CASE == 11
    void rejected(Bundle& bundle, const vsl::ComputePhase& phase) {
        bundle.update(phase, [](Bundle::Frame&) { return 7; });
    }
#elif VSL_TYPED_CASE == 12
    using Two = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT>, t::TextureBinding<3, vsl::ShaderFlag::FRAGMENT>>>, t::ExternalAllocation<1, 2>, t::ExternalAllocation<1, 3>>;
    void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors, const t::TypedTexture& texture) {
        t::TypedResourceBundle<Two> bundle(manager, descriptors, t::external_resource<1, 2>(texture), t::external_resource<1, 2>(texture));
    }
#elif VSL_TYPED_CASE == 13
    void rejected(Bundle& bundle) {
        Bundle copy(bundle);
    }
#elif VSL_TYPED_CASE == 14
    void rejected(const Bundle::Frame& frame) {
        Bundle::Frame copy(frame);
    }
#endif
} // namespace
