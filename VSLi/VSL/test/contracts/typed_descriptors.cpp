#include <VSL/utils/typed_pipeline.hpp>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    struct Payload {
        float rgba[4];
    };

    using PayloadRecord = t::BlockRecord<Payload, VSL_BLOCK_MEMBER(Payload, rgba)>;
    struct Wrong {
        float rgba[4];
    };
    using Uniform = t::UniformBinding<3, PayloadRecord, vsl::ShaderFlag::VERTEX, 2>;
    using Set = t::DescriptorSet<2, Uniform>;
    using Schema = t::ResourceSchema<Set>;
    using Pipeline = t::TypedGraphicsPipeline<t::TypedVertexInput<>, Schema>;

#if VSL_TYPED_CASE == 0
    using Value = t::detail::DescriptorValue;
    static_assert(!std::is_aggregate_v<Value>);
    static_assert(std::is_default_constructible_v<Value> && std::is_copy_constructible_v<Value> && std::is_move_constructible_v<Value>);
    static_assert(!std::is_constructible_v<Value, vsl::lifetime::OwnedRef<vsl::impl::BufferImpl>>);
    void factories(vsl::BufferView buffer, vsl::ImageAccessor image, vsl::SamplerAccessor sampler, vsl::ResourceUse use) {
        auto value = Value::FromBuffer(buffer, use);
        value.buffer_subview(0, sizeof(Payload));
        Value::FromImage(image, use).image();
        Value::FromSampler(sampler, use).sampler();
        Value::FromTexture(image, sampler, use).resource_use();
    }
    void accepted(Pipeline& pipeline, t::TypedDescriptorSet<Set>& set, t::TypedUniformBuffer<Payload>& buffer) {
        set.write<3>(std::array{buffer, buffer});
        auto draw = pipeline.bind_vertices().bind_resources(set).draw(3);
    }
    static_assert(t::compatible_descriptor_sets<Schema, t::TypedDescriptorSet<Set>>);
#elif VSL_TYPED_CASE == 1
    void rejected(t::TypedDescriptorSet<Set>& set, t::TypedUniformBuffer<Wrong>& buffer) {
        set.write<3>(std::array{buffer, buffer});
    }
#elif VSL_TYPED_CASE == 2
    void rejected(t::TypedDescriptorSet<Set>& set, t::TypedStorageBuffer<Payload>& buffer) {
        set.write<3>(std::array{buffer, buffer});
    }
#elif VSL_TYPED_CASE == 3
    void rejected(t::TypedDescriptorSet<Set>& set, t::TypedUniformBuffer<Payload>& buffer) {
        set.write<3>(std::array{buffer});
    }
#elif VSL_TYPED_CASE == 4
    void rejected(t::TypedDescriptorSet<Set>& set, t::TypedUniformBuffer<Payload>& buffer) {
        set.write<9>(std::array{buffer, buffer});
    }
#elif VSL_TYPED_CASE == 5
    using Other = t::DescriptorSet<1, t::UniformBinding<4, PayloadRecord, vsl::ShaderFlag::VERTEX, 2>>;
    void rejected(Pipeline& pipeline, t::TypedDescriptorSet<Other>& set) {
        pipeline.bind_vertices().bind_resources(set);
    }
#elif VSL_TYPED_CASE == 6
    void rejected(Pipeline& pipeline) {
        pipeline.bind_vertices().bind_resources();
    }
#elif VSL_TYPED_CASE == 7
    void rejected(Pipeline& pipeline) {
        pipeline.bind_vertices().draw(3);
    }
#elif VSL_TYPED_CASE == 8
    static_assert(t::DescriptorSet<0, Uniform, Uniform>::BINDING_COUNT == 2);
#elif VSL_TYPED_CASE == 9
    static_assert(t::ResourceSchema<Set, Set>::COUNT == 2);
#elif VSL_TYPED_CASE == 10
    using Invalid = t::UniformBinding<0, PayloadRecord, vsl::ShaderFlag::VERTEX, 0>;
    static_assert(Invalid::POINT.amount == 0);
#elif VSL_TYPED_CASE == 11
    void rejected(Pipeline& pipeline, vsl::GraphicResource& set) {
        pipeline.bind_vertices().bind_resources(set);
    }
#elif VSL_TYPED_CASE == 12
    void rejected(t::TypedDescriptorSet<Set>& set, vsl::StagingBuffer<vsl::MemoryType::UNIFORM_BUFFER>& buffer) {
        set.write<3>(std::array{buffer, buffer});
    }
#elif VSL_TYPED_CASE == 13
    void rejected(t::detail::DescriptorValue value) {
        value._buffer.reset();
    }
#elif VSL_TYPED_CASE == 14
    void rejected(vsl::lifetime::OwnedRef<vsl::impl::BufferImpl> buffer) {
        t::detail::DescriptorValue value{buffer, {}, {}, sizeof(Payload), 0, {}};
    }
#endif
} // namespace
