#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include <AHO/pipeline/layouts.hpp>
namespace {
    using AHOLayoutsVulkanIntegration = aho_test::LiveGPUTest;
    TEST_F(AHOLayoutsVulkanIntegration, BaseLayoutFactoryCreatesLayoutUsingEngineDevice) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_NE(aho::pipeline::GET_BASE_LAYOUT, nullptr);
        const auto layout = aho::pipeline::GET_BASE_LAYOUT(engine.get());
        ASSERT_NE(layout.data_, nullptr);
        EXPECT_EQ(layout.data_->device, engine->data()->logical_device.data_);
        EXPECT_TRUE(static_cast<bool>(layout.data_->pipeline_layout));
        ASSERT_NE(layout.data_->info, nullptr);
        EXPECT_EQ(layout.data_->info->rasterization.cullMode, vk::CullModeFlags(vk::CullModeFlagBits::eBack));
        EXPECT_EQ(layout.data_->info->input_assembly.topology, vk::PrimitiveTopology::eTriangleList);
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/pipeline/layouts.hpp>
namespace {
    TEST(AHOLayoutsTest, RuntimeAndTemplateBindingLookupsAgreeOnResourceContracts) {
        using namespace aho::pipeline;
        const auto matrix = get_binding_point(ResourceName::VP_MATRIX_UBO);
        constexpr auto static_matrix = get_binding_point<ResourceName::VP_MATRIX_UBO>();
        static_assert(static_matrix.binding_type == vsl::graphic_resource::Type::UNIFORM_BUFFER);
        static_assert(static_matrix.shader_type == vsl::ShaderFlag::VERTEX && static_matrix.amount == 1);
        EXPECT_EQ(matrix.binding_type, vsl::graphic_resource::Type::UNIFORM_BUFFER);
        EXPECT_EQ(matrix.shader_type, vsl::ShaderFlag::VERTEX);
        EXPECT_EQ(matrix.amount, 1u);
        EXPECT_EQ(matrix.binding, static_matrix.binding);
        const auto texture = get_binding_point(ResourceName::TEXTURE);
        constexpr auto static_texture = get_binding_point<ResourceName::TEXTURE>();
        static_assert(static_texture.binding_type == vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER);
        static_assert(static_texture.shader_type == vsl::ShaderFlag::FRAGMENT && static_texture.amount == 1);
        EXPECT_EQ(texture.binding_type, vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER);
        EXPECT_EQ(texture.shader_type, vsl::ShaderFlag::FRAGMENT);
        EXPECT_EQ(texture.amount, 1u);
        EXPECT_EQ(texture.binding, static_texture.binding);
        EXPECT_THROW(get_binding_point(static_cast<ResourceName>(999)), std::runtime_error);
    }
} // namespace
#endif
