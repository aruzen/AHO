#include <VSL/utils/typed_resource_bundle.hpp>
#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif

namespace {
    namespace t = vsl::typed;
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using Allocation = t::ResourceAllocationSchema<t::ResourceSchema<Set>, t::BufferAllocation<0, 0>>;
    using Default = t::TypedResourceBundle<Allocation>;
    struct Policy : t::DefaultResourceStoragePolicy {};
    using Custom = t::TypedResourceBundle<Allocation, Policy>;
    static_assert(std::same_as<Default, t::TypedResourceBundle<Allocation, t::DefaultResourceStoragePolicy>>);
    static_assert(std::same_as<Default::Frame, t::TypedResourceFrame<Allocation>>);
    static_assert(std::same_as<Default::Shared, t::TypedSharedResources<Allocation>>);
    static_assert(std::same_as<decltype(std::declval<Default::Frame&>().buffer<0, 0>()), t::TypedUniformBuffer<std::uint32_t>&>);
    static_assert(std::same_as<decltype(std::declval<const Custom::Frame&>().buffer<0, 0>()), const t::TypedUniformBuffer<std::uint32_t>&>);
    static_assert(std::is_nothrow_move_constructible_v<Default::Frame> && std::is_nothrow_move_constructible_v<Default>);
    struct ThrowingMovePolicy {
        template<typename Binding, typename Spec>
        struct Buffer : t::TypedUniformBuffer<std::uint32_t> {
            Buffer(Buffer&&) noexcept(false) = default;
        };
        template<typename Binding, typename Spec>
        Buffer<Binding, Spec> make_buffer(vsl::LogicalDeviceAccessor, size_t, size_t);
    };
    static_assert(!std::is_nothrow_move_constructible_v<t::TypedResourceFrame<Allocation, ThrowingMovePolicy>>);
    struct InvalidPolicy {
#if VSL_TYPED_CASE == 1
#elif VSL_TYPED_CASE == 2
        template<typename Binding, typename Spec>
        using Buffer = t::TypedUniformBuffer<float>;
        template<typename Binding, typename Spec>
        Buffer<Binding, Spec> make_buffer(vsl::LogicalDeviceAccessor, size_t, size_t);
#elif VSL_TYPED_CASE == 3
        template<typename Binding, typename Spec>
        using Buffer = t::TypedStorageBuffer<std::uint32_t>;
        template<typename Binding, typename Spec>
        Buffer<Binding, Spec> make_buffer(vsl::LogicalDeviceAccessor, size_t, size_t);
#elif VSL_TYPED_CASE == 4
        template<typename Binding, typename Spec>
        using Buffer = t::TypedUniformBuffer<std::uint32_t>;
        template<typename Binding, typename Spec>
        int make_buffer(vsl::LogicalDeviceAccessor, size_t, size_t);
#elif VSL_TYPED_CASE == 5
        template<typename Binding, typename Spec>
        using Buffer = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
        template<typename Binding, typename Spec>
        Buffer<Binding, Spec> make_buffer(vsl::LogicalDeviceAccessor, size_t, size_t);
#else
        template<typename Binding, typename Spec>
        using Buffer = t::TypedUniformBuffer<std::uint32_t>;
#endif
    };
#if VSL_TYPED_CASE == 0
    [[maybe_unused]] void accepted(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors) {
        Default first(manager, descriptors);
        Default second(manager, descriptors, {});
        Custom third(Policy{}, manager, descriptors);
        Custom fourth(Policy{}, manager, descriptors, {});
        using External = t::ResourceAllocationSchema<t::ResourceSchema<Set>, t::ExternalAllocation<0, 0>>;
        struct NoFactory {};
        const std::uint32_t value = 7;
        t::TypedUniformBuffer<std::uint32_t> input(manager.get_device(), value);
        t::TypedResourceBundle<External, NoFactory> fifth(manager, descriptors, {}, t::external_resource<0, 0>(input));
    }
#elif VSL_TYPED_CASE == 7
    [[maybe_unused]] void rejected(Custom& bundle, const vsl::ComputePhase& phase) {
        bundle.update(phase, [](Default::Frame&) {});
    }
#elif VSL_TYPED_CASE == 8
    struct MoveOnly : t::DefaultResourceStoragePolicy {
        explicit MoveOnly(int) {}
        MoveOnly(MoveOnly&&) = default;
        MoveOnly(const MoveOnly&) = delete;
    };
    [[maybe_unused]] void rejected(vsl::CommandManager manager, vsl::graphic_resource::ManagerInterface& descriptors) {
        t::TypedResourceBundle<Allocation, MoveOnly> bundle(manager, descriptors);
    }
#elif VSL_TYPED_CASE == 9
    [[maybe_unused]] void rejected(Custom::Frame& frame) {
        const float value = 1;
        frame.buffer<0, 0>().copy(std::span(&value, 1));
    }
#else
    static_assert(sizeof(t::TypedResourceBundle<Allocation, InvalidPolicy>) > 0);
#endif
} // namespace
