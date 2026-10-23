#include <VSL/utils/typed_block_validation.hpp>
#include <VSL/utils/typed_compute.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    using Reflector = vsl::utils::SPIRVReflector;
    struct alignas(16) Column {
        float xyz[3];
        float padding;
        static constexpr auto GRAPHIC_TYPE = vsl::data_format::FLOAT_RGB;
    };
    struct alignas(16) Weight {
        float value, padding[3];
        static constexpr auto GRAPHIC_TYPE = vsl::data_format::FLOAT;
    };
    struct alignas(16) Inner {
        float uv[2];
        std::uint32_t code, padding;
    };
    using InnerLayout = t::BlockRecord<Inner, VSL_BLOCK_MEMBER(Inner, uv), VSL_BLOCK_MEMBER(Inner, code)>;
    struct alignas(16) Uniforms {
        float color[4];
        std::array<Column, 3> basis;
        std::array<Weight, 3> weights;
        std::array<Inner, 2> nested;
    };
    using UniformLayout = t::BlockRecord<Uniforms, VSL_BLOCK_MEMBER(Uniforms, color),
                                         VSL_BLOCK_MEMBER(Uniforms, basis, t::BlockMatrix<float, 3, 3, 16>), VSL_BLOCK_MEMBER(Uniforms, weights),
                                         VSL_BLOCK_MEMBER(Uniforms, nested, t::BlockArray<InnerLayout, 2>)>;
    struct PushData {
        float transform[2][2];
        std::uint32_t index;
    };
    using PushLayout = t::BlockRecord<PushData, VSL_BLOCK_MEMBER(PushData, transform, t::BlockMatrix<float, 2, 2, 8, true>), VSL_BLOCK_MEMBER(PushData, index)>;
    struct alignas(16) WrongInner {
        float uv[2];
        std::uint32_t padding, code;
    };
    using WrongInnerLayout = t::BlockRecord<WrongInner, VSL_BLOCK_MEMBER(WrongInner, uv), VSL_BLOCK_MEMBER(WrongInner, code)>;
    struct alignas(16) WrongOffset {
        float color[4];
        std::array<Column, 3> basis;
        std::array<Weight, 3> weights;
        std::array<WrongInner, 2> nested;
    };
    using WrongOffsetLayout = t::BlockRecord<WrongOffset, VSL_BLOCK_MEMBER(WrongOffset, color),
                                             VSL_BLOCK_MEMBER(WrongOffset, basis, t::BlockMatrix<float, 3, 3, 16>), VSL_BLOCK_MEMBER(WrongOffset, weights),
                                             VSL_BLOCK_MEMBER(WrongOffset, nested, t::BlockArray<WrongInnerLayout, 2>)>;
    struct alignas(16) PackedWeights {
        float color[4];
        std::array<Column, 3> basis;
        std::array<float, 3> weights;
        std::array<Inner, 2> nested;
    };
    using PackedWeightsLayout = t::BlockRecord<PackedWeights, VSL_BLOCK_MEMBER(PackedWeights, color),
                                               VSL_BLOCK_MEMBER(PackedWeights, basis, t::BlockMatrix<float, 3, 3, 16>), VSL_BLOCK_MEMBER(PackedWeights, weights),
                                               VSL_BLOCK_MEMBER(PackedWeights, nested, t::BlockArray<InnerLayout, 2>)>;
    struct alignas(16) PackedMatrix {
        float color[4];
        float basis[3][3];
        std::array<Weight, 3> weights;
        std::array<Inner, 2> nested;
    };
    using PackedMatrixLayout = t::BlockRecord<PackedMatrix, VSL_BLOCK_MEMBER(PackedMatrix, color),
                                              VSL_BLOCK_MEMBER(PackedMatrix, basis, t::BlockMatrix<float, 3, 3, 12>), VSL_BLOCK_MEMBER(PackedMatrix, weights),
                                              VSL_BLOCK_MEMBER(PackedMatrix, nested, t::BlockArray<InnerLayout, 2>)>;

    struct alignas(8) Vector2 {
        float xy[2];
        static constexpr auto GRAPHIC_TYPE = vsl::data_format::FLOAT_RG;
    };
    struct Matrix2 {
        float columns[2][2];
    };
    using Matrix2Layout = t::BlockValue<Matrix2, t::BlockMatrix<float, 2, 2, 8>>;
    struct alignas(8) Arrays {
        std::uint32_t count;
        std::array<float, 3> weights;
        std::array<std::array<Vector2, 3>, 2> points;
        std::array<Matrix2, 2> transforms;
        float rows[3][2], columns[3][2];
    };
    using ArraysLayout = t::BlockRecord<Arrays, VSL_BLOCK_MEMBER(Arrays, count), VSL_BLOCK_MEMBER(Arrays, weights),
                                        VSL_BLOCK_MEMBER(Arrays, points), VSL_BLOCK_MEMBER(Arrays, transforms, t::BlockArray<Matrix2Layout, 2>),
                                        VSL_BLOCK_MEMBER(Arrays, rows, t::BlockMatrix<float, 2, 3, 8, true>),
                                        VSL_BLOCK_MEMBER(Arrays, columns, t::BlockMatrix<float, 3, 2, 8, false>)>;
    struct Half {
        std::uint16_t bits;
        static constexpr auto GRAPHIC_TYPE = vsl::data_format::FLOAT16;
    };
    struct Numeric {
        std::int8_t signed_byte;
        std::uint16_t unsigned_short;
        Half half_value;
        float value;
        double wide;
    };
    using NumericLayout = t::BlockRecord<Numeric, VSL_BLOCK_MEMBER(Numeric, signed_byte), VSL_BLOCK_MEMBER(Numeric, unsigned_short),
                                         VSL_BLOCK_MEMBER(Numeric, half_value), VSL_BLOCK_MEMBER(Numeric, value), VSL_BLOCK_MEMBER(Numeric, wide)>;

    const Reflector::Block& uniform_block(const Reflector& reflection);
    template<typename Layout>
    void expect_layout_error(const Reflector::Block& block, const char* reason);
    class TypedBlockVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    const Reflector::Block& uniform_block(const Reflector& reflection) {
        for (const auto& block : reflection.generated.blocks)
            if (block.kind == vsl::graphic_resource::Type::UNIFORM_BUFFER)
                return block;
        throw std::logic_error("Uniform block fixture is missing");
    }
    template<typename Layout>
    void expect_layout_error(const Reflector::Block& block, const char* reason) {
        try {
            t::validate_block_layout<Layout>(block);
            FAIL() << "Invalid C++ layout was accepted";
        } catch (const std::invalid_argument& error) {
            EXPECT_NE(std::string(error.what()).find(reason), std::string::npos) << error.what();
        }
    }

    TEST(TypedBlockLayoutTest, ReflectsOffsetsStridesNestedStructuresAndMatrixMajorOrder) {
        Reflector reflection({}, vsl_test::shader_path("typed_block_layout.comp.spv"));
        ASSERT_EQ(reflection.generated.blocks.size(), 3u);
        const auto& block = uniform_block(reflection);
        ASSERT_EQ(block.type.members.size(), 4u);
        EXPECT_EQ(block.declared_size, sizeof(Uniforms));
        EXPECT_EQ(block.type.members[1].offset, offsetof(Uniforms, basis));
        EXPECT_EQ(block.type.members[1].type.stride, 16u);
        EXPECT_EQ(block.type.members[2].type.stride, sizeof(Weight));
        EXPECT_EQ(block.type.members[3].type.stride, sizeof(Inner));
        EXPECT_NO_THROW(t::validate_block_layout<UniformLayout>(block));
        auto copied = reflection.generated;
        EXPECT_NO_THROW(t::validate_block_layout<UniformLayout>(copied.blocks.front()));
        for (const auto& push : reflection.generated.blocks)
            if (!push.kind) {
                EXPECT_TRUE(push.type.members[0].type.row_major);
                EXPECT_NO_THROW(t::validate_block_layout<PushLayout>(push));
            }
    }
    TEST(TypedBlockLayoutTest, EqualRecordSizesDoNotHideMemberOffsetOrMatrixStrideMismatch) {
        static_assert(sizeof(WrongOffset) == sizeof(Uniforms));
        static_assert(sizeof(PackedMatrix) == sizeof(Uniforms));
        Reflector reflection({}, vsl_test::shader_path("typed_block_layout.comp.spv"));
        const auto& block = uniform_block(reflection);
        expect_layout_error<WrongOffsetLayout>(block, "offset");
        expect_layout_error<PackedMatrixLayout>(block, "matrix stride");
        expect_layout_error<PackedWeightsLayout>(block, "array stride");
    }
    TEST(TypedBlockLayoutTest, MatrixMajorOrderIsCheckedEvenForSquareMatricesWithIdenticalSizes) {
        using WrongMajor = t::BlockRecord<PushData, VSL_BLOCK_MEMBER(PushData, transform, t::BlockMatrix<float, 2, 2, 8, false>), VSL_BLOCK_MEMBER(PushData, index)>;
        Reflector reflection({}, vsl_test::shader_path("typed_block_layout.comp.spv"));
        for (const auto& block : reflection.generated.blocks)
            if (!block.kind)
                expect_layout_error<WrongMajor>(block, "major");
    }

    TEST(TypedBlockLayoutTest, MultidimensionalArraysMatrixArraysAndRectangularMajorOrderArePreserved) {
        Reflector reflection({}, vsl_test::shader_path("typed_block_arrays.comp.spv"));
        ASSERT_EQ(reflection.generated.blocks.size(), 1u);
        const auto& block = reflection.generated.blocks.front();
        ASSERT_EQ(block.type.members.size(), 6u);
        const auto& points = block.type.members[2].type;
        EXPECT_EQ(points.count, 2u);
        EXPECT_EQ(points.stride, sizeof(std::array<Vector2, 3>));
        ASSERT_EQ(points.elements.size(), 1u);
        EXPECT_EQ(points.elements.front().count, 3u);
        EXPECT_EQ(points.elements.front().stride, sizeof(Vector2));
        EXPECT_EQ(block.type.members[3].type.stride, sizeof(Matrix2));
        EXPECT_NO_THROW(t::validate_block_layout<ArraysLayout>(block));
    }
    TEST(TypedBlockLayoutTest, NumericStorageWidthsAreReflectedWithoutVertexFormatConversions) {
        Reflector reflection({}, vsl_test::shader_path("typed_block_numeric.comp.spv"));
        ASSERT_EQ(reflection.generated.blocks.size(), 1u);
        const auto& block = reflection.generated.blocks.front();
        ASSERT_EQ(block.type.members.size(), 5u);
        const std::array<std::uint32_t, 5> widths{8, 16, 16, 32, 64};
        for (size_t i = 0; i < widths.size(); ++i)
            EXPECT_EQ(block.type.members[i].type.width, widths[i]);
        EXPECT_NO_THROW(t::validate_block_layout<NumericLayout>(block));
    }

    TEST(TypedBlockLayoutTest, RuntimeStorageElementsMustMatchNumericTypeAndElementStride) {
        struct Padded {
            std::uint32_t value, padding;
        };
        using PaddedLayout = t::BlockRecord<Padded, VSL_BLOCK_MEMBER(Padded, value)>;
        Reflector reflection({}, vsl_test::shader_path("typed_block_layout.comp.spv"));
        for (const auto& block : reflection.generated.blocks)
            if (block.kind == vsl::graphic_resource::Type::STORAGE_BUFFER) {
                EXPECT_NO_THROW(t::validate_block_layout<std::uint32_t>(block));
                expect_layout_error<float>(block, "numeric");
                expect_layout_error<PaddedLayout>(block, "storage element sizeof/array stride");
            }
    }

    TEST_F(TypedBlockVulkanIntegrationTest, TypedUniformNestedArraysMatricesAndPushDataReachTheComputeShader) {
        using Input = t::DescriptorSet<0, t::UniformBinding<2, UniformLayout, vsl::ShaderFlag::COMPUTE>>;
        using Output = t::DescriptorSet<1, t::StorageBinding<4, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using Resources = t::ResourceSchema<Input, Output>;
        using Pushes = t::PushSchema<t::PushRange<PushLayout, vsl::ShaderFlag::COMPUTE>>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_block_layout.comp.spv"));
        t::TypedComputePipeline<Resources, Pushes> pipeline(device, vsl::pipeline_layout::ShaderGroup("block layouts", {shader}));
        Uniforms data{};
        data.color[0] = 1;
        data.basis[2].xyz[1] = 2;
        data.weights[1].value = 3;
        data.nested[1].uv[1] = 4;
        data.nested[1].code = 5;
        const PushData push{{{1, 6}, {7, 1}}, 7};
        const std::uint32_t zero = 0;
        t::TypedUniformBuffer<UniformLayout> input(device, data);
        t::TypedStorageBuffer<std::uint32_t> output(device, zero);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Input::Layout> input_set(device, manager);
        t::TypedDescriptorSet<Output::Layout> output_set(device, manager);
        input_set.write<2>(input);
        output_set.write<4>(output);
        auto commands = make_manager();
        {
            auto phase = commands.start_phase<vsl::ComputePhase>();
            phase << pipeline.bind_resources(input_set, output_set).push_constants(Pushes::Values(push)).dispatch(1);
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto buffer = commands.get_buffer();
            buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = output.backend().data();
        EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), 28u);
    }

    TEST_F(TypedBlockVulkanIntegrationTest, WrongCpuLayoutsAreRejectedDuringPipelineCreation) {
        using Output = t::DescriptorSet<1, t::StorageBinding<4, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        using BadInput = t::DescriptorSet<0, t::UniformBinding<2, PackedMatrixLayout, vsl::ShaderFlag::COMPUTE>>;
        using Resources = t::ResourceSchema<BadInput, Output>;
        using Pushes = t::PushSchema<t::PushRange<PushLayout, vsl::ShaderFlag::COMPUTE>>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_block_layout.comp.spv"));
        EXPECT_THROW((t::TypedComputePipeline<Resources, Pushes>(device, vsl::pipeline_layout::ShaderGroup("invalid block layouts", {shader}))), std::invalid_argument);
        EXPECT_FALSE(has_validation_errors());
    }
    TEST_F(TypedBlockVulkanIntegrationTest, Std430ScalarArraysMultidimensionalVectorsAndMatricesAreReadAndWritten) {
        using Data = t::DescriptorSet<0, t::StorageBinding<0, ArraysLayout, vsl::ShaderFlag::COMPUTE>>;
        using Resources = t::ResourceSchema<Data>;
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_block_arrays.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("array layouts", {shader}));
        Arrays data{};
        data.count = 2;
        data.weights[2] = 3;
        data.points[1][2].xy[1] = 5;
        data.transforms[1].columns[0][1] = 7;
        data.rows[2][1] = 11;
        data.columns[2][1] = 13;
        t::TypedStorageBuffer<ArraysLayout> buffer(device, data);
        vsl::GraphicResourceManager manager(device);
        t::TypedDescriptorSet<Data::Layout> set(device, manager);
        set.write<0>(buffer);
        auto commands = make_manager();
        {
            auto phase = commands.start_phase<vsl::ComputePhase>();
            phase << pipeline.bind_resources(set).dispatch(1);
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto native = commands.get_buffer();
            native.data_->command_buffers[native.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = buffer.backend().data();
        EXPECT_EQ(*static_cast<const std::uint32_t*>(mapped.data), 41u);
    }
} // namespace
