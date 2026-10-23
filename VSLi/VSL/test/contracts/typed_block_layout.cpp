#include <VSL/utils/typed_resources.hpp>
#include <VSL/utils/typed_push_constants.hpp>

#ifndef VSL_TYPED_CASE
#define VSL_TYPED_CASE 0
#endif
namespace {
    namespace t = vsl::typed;
    struct Data {
        float color[4];
        float matrix[3][4];
        std::array<float, 4> weights;
    };
    struct Other {
        float color[4];
    };
    using Layout = t::BlockRecord<Data, VSL_BLOCK_MEMBER(Data, color),
                                  VSL_BLOCK_MEMBER(Data, matrix, t::BlockMatrix<float, 3, 3, 16>),
                                  VSL_BLOCK_MEMBER(Data, weights)>;
#if VSL_TYPED_CASE == 0
    using Binding = t::UniformBinding<0, Layout, vsl::ShaderFlag::VERTEX>;
    using Push = t::PushSchema<t::PushRange<Layout, vsl::ShaderFlag::VERTEX>>;
    static_assert(std::same_as<Binding::Payload, Data>);
    static_assert(std::same_as<t::TypedUniformBuffer<Layout>::Payload, Data>);
    static_assert(Push::DATA_SIZE == sizeof(Data));
    void accepted(vsl::LogicalDeviceAccessor device, const Data& value) {
        t::TypedUniformBuffer<Layout> buffer(device, value);
        Push::Values values(value);
    }
#elif VSL_TYPED_CASE == 1
    using Bad = t::UniformBinding<0, Data, vsl::ShaderFlag::VERTEX>;
    static_assert(Bad::PAYLOAD_SIZE > 0);
#elif VSL_TYPED_CASE == 2
    using Bad = t::PushRange<Data, vsl::ShaderFlag::VERTEX>;
    static_assert(Bad::RANGE.size > 0);
#elif VSL_TYPED_CASE == 3
    using Bad = t::BlockRecord<Data, VSL_BLOCK_MEMBER(Other, color)>;
    static_assert(Bad::LAYOUT.size > 0);
#elif VSL_TYPED_CASE == 4
    using Bad = t::BlockMember<Data, float, 0, t::BlockVector<float, 4>>;
    static_assert(Bad::FIELD.offset == 0);
#elif VSL_TYPED_CASE == 5
    using Bad = t::BlockMember<Data, float, sizeof(Data)>;
    static_assert(Bad::FIELD.offset > 0);
#elif VSL_TYPED_CASE == 6
    static_assert(t::BlockMatrix<float, 1, 3>::LAYOUT.size > 0);
#elif VSL_TYPED_CASE == 7
    static_assert(t::BlockMatrix<int, 3, 3>::LAYOUT.size > 0);
#elif VSL_TYPED_CASE == 8
    static_assert(t::BlockMatrix<float, 3, 3, 8>::LAYOUT.size > 0);
#elif VSL_TYPED_CASE == 9
    static_assert(t::BlockArray<float, std::numeric_limits<size_t>::max()>::LAYOUT.size > 0);
#elif VSL_TYPED_CASE == 10
    static_assert(t::BlockArray<float, 0>::LAYOUT.size > 0);
#elif VSL_TYPED_CASE == 11
    void rejected(vsl::LogicalDeviceAccessor device, const Other& value) {
        t::TypedUniformBuffer<Layout> buffer(device, value);
    }
#elif VSL_TYPED_CASE == 12
    struct Short {
        float value;
        static constexpr auto GRAPHIC_TYPE = vsl::data_format::FLOAT_RGBA;
    };
    using Bad = t::UniformBinding<0, Short, vsl::ShaderFlag::VERTEX>;
    static_assert(Bad::PAYLOAD_SIZE > 0);
#endif
} // namespace
