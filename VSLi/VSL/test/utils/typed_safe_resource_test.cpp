#include "support/vulkan_fixture.hpp"
#include <VSL/utils/typed_safe_resource.hpp>

namespace {
    template<typename Point, typename Image>
    concept has_shader_read_barrier = requires(const Image& image) {
        { Point::MakeShaderReadBarrier(image) } -> std::same_as<vsl::command::ChangeImageBarrier>;
    };

    using SampledImage = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST>;
    using TransferImage = vsl::Image<vsl::ImageType::TRANSFER_DST>;
    using SampledPoint = vsl::typed::ResourceBindingPoint<2, vsl::graphic_resource::Type::SAMPLED_IMAGE, vsl::ShaderFlag::FRAGMENT>;
    using TexturePoint = vsl::typed::ResourceBindingPoint<3, vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, vsl::ShaderFlag::COMPUTE>;
    using StoragePoint = vsl::typed::ResourceBindingPoint<4, vsl::graphic_resource::Type::STORAGE_IMAGE, vsl::ShaderFlag::COMPUTE>;
    using EmptyStagesPoint = vsl::typed::ResourceBindingPoint<5, vsl::graphic_resource::Type::SAMPLED_IMAGE, vsl::ShaderFlag::ERROR_>;
    using UnknownStagesPoint = vsl::typed::ResourceBindingPoint<6, vsl::graphic_resource::Type::SAMPLED_IMAGE, static_cast<vsl::ShaderFlag>(0x40000000)>;
    static_assert(has_shader_read_barrier<SampledPoint, SampledImage>);
    static_assert(has_shader_read_barrier<TexturePoint, SampledImage>);
    static_assert(!has_shader_read_barrier<SampledPoint, TransferImage>);
    static_assert(!has_shader_read_barrier<StoragePoint, SampledImage>);
    static_assert(!has_shader_read_barrier<EmptyStagesPoint, SampledImage>);
    static_assert(!has_shader_read_barrier<UnknownStagesPoint, SampledImage>);
    static_assert(!has_shader_read_barrier<SampledPoint, vsl::ImageAccessor>);

    using First =
        vsl::typed::ResourceBindingPoint<2, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX>;
    using Second = vsl::typed::ResourceBindingPoint<5, vsl::graphic_resource::Type::STORAGE_BUFFER,
                                                    vsl::ShaderFlag::COMPUTE, 3>;
    using Points = vsl::typed::ResourceBindingPoints<First, Second>;
    static_assert(Points::Size() == 2 && Points::Get<1>().binding == 5);
    TEST(TypedResourceTest, BindingDefinitionsRetainIndexTypeStageAndArrayCount) {
        const auto point = Points::Get<1>();
        EXPECT_EQ(point.binding, 5u);
        EXPECT_EQ(point.binding_type, vsl::graphic_resource::Type::STORAGE_BUFFER);
        EXPECT_EQ(point.shader_type, vsl::ShaderFlag::COMPUTE);
        EXPECT_EQ(point.amount, 3u);
    }
    TEST(TypedResourceTest, VertexAndPushDefinitionsRetainExplicitLayout) {
        using Attribute = vsl::typed::VertexInputDefinition<vsl::data_format::FLOAT_RG, 3, 8>;
        using Shape = vsl::typed::VertexShapeDefinition<
            4, 16, vsl::pipeline_layout::VertexInputShapeDefinition::UpdateTiming::NEXT_INSTANCE, Attribute>;
        EXPECT_EQ(Shape::Size(), 1u);
        EXPECT_EQ(Shape::Get<0>().format, vsl::data_format::FLOAT_RG);
        EXPECT_EQ(Shape::Get<0>().location, 3u);
        EXPECT_EQ(Shape::Get<0>().offset, 8u);
        using Push = vsl::typed::PushContentDefinitionTag<vsl::ShaderFlag::FRAGMENT, 16, 32>;
        using Definitions = vsl::typed::PushContentDefinitions<Push>;
        EXPECT_EQ(Definitions::Size(), 1u);
        EXPECT_EQ(Definitions::Get<0>().target_shader, vsl::ShaderFlag::FRAGMENT);
        EXPECT_EQ(Definitions::Get<0>().offset, 32u);
        EXPECT_EQ(Definitions::Get<0>().size, 16u);
        EXPECT_EQ(vsl::typed::ResourceBindingPoints<>::Size(), 0u);
        EXPECT_EQ(vsl::typed::PushContentDefinitions<>::Size(), 0u);
    }
    using TypedResourceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(TypedResourceVulkanIntegrationTest, MakesBindingLayoutFromTypedPoints) {
        const auto layout = Points{}.make(device);
        const auto actual = layout.get_binding_points();
        ASSERT_EQ(actual.size(), 2u);
        EXPECT_EQ(actual[0], First::VALUE);
        EXPECT_EQ(actual[1], Second::VALUE);
    }

    TEST_F(TypedResourceVulkanIntegrationTest, SampledBindingBuildsShaderReadBarrierWithDeclaredStagesAndLayers) {
        SampledImage image(device, 1, 1, vsl::data_format::SRGB8_RGBA, 3);
        const auto fragment = SampledPoint::MakeShaderReadBarrier(image, 1, 2);
        const auto compute = TexturePoint::MakeShaderReadBarrier(image);
        EXPECT_EQ(fragment.shader_read_stages(), SampledPoint::VALUE.shader_type);
        EXPECT_EQ(compute.shader_read_stages(), TexturePoint::VALUE.shader_type);
        EXPECT_EQ(fragment.old_layout, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
        EXPECT_EQ(fragment.new_layout, vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        EXPECT_EQ(fragment.image.data_, image.data_);
        EXPECT_EQ(fragment.base_layer, 1u);
        EXPECT_EQ(fragment.layer_count, 2u);
        EXPECT_EQ(compute.base_layer, 0u);
        EXPECT_EQ(compute.layer_count, 0u);
    }
} // namespace
namespace {
    TEST_F(TypedResourceVulkanIntegrationTest, TypedVertexInputInjectsItsDeclaredAttribute) {
        using Attribute = vsl::typed::VertexInputDefinition<vsl::data_format::FLOAT_RG, 3, 8>;
        using Shape = vsl::typed::VertexShapeDefinition<
            4, 16, vsl::pipeline_layout::VertexInputShapeDefinition::UpdateTiming::NEXT_INSTANCE, Attribute>;
        auto layout = make_layout_state();
        vsl::typed::TypedVertexInput<Shape>{}.injection(layout);
        const auto& state = layout.data_->info->vertex_input;
        ASSERT_EQ(state.vertexBindingDescriptionCount, 1u);
        ASSERT_EQ(state.vertexAttributeDescriptionCount, 1u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].binding, 4u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].stride, 16u);
        EXPECT_EQ(state.pVertexBindingDescriptions[0].inputRate, vk::VertexInputRate::eInstance);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].location, 3u);
        EXPECT_EQ(state.pVertexAttributeDescriptions[0].offset, 8u);
    }
} // namespace
