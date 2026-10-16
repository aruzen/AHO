#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include <AHO/pipeline/layouts.hpp>
namespace {
using AhoLayoutsVulkanIntegration = aho_test::LiveGpuTest;
TEST_F(AhoLayoutsVulkanIntegration, BaseLayoutFactoryCreatesLayoutUsingEngineDevice) {
    ASSERT_NO_THROW(start_engine());
    ASSERT_NE(aho::pipeline::GET_BASE_LAYOUT, nullptr);
    const auto layout = aho::pipeline::GET_BASE_LAYOUT(engine.get());
    ASSERT_NE(layout._data, nullptr);
    EXPECT_EQ(layout._data->device, engine->_data->logical_device._data);
    EXPECT_TRUE(static_cast<bool>(layout._data->pipelineLayout));
    ASSERT_NE(layout._data->info, nullptr);
    EXPECT_EQ(layout._data->info->rasterization.cullMode, vk::CullModeFlags(vk::CullModeFlagBits::eBack));
    EXPECT_EQ(layout._data->info->inputAssembly.topology, vk::PrimitiveTopology::eTriangleList);
}
}
#else
#include <gtest/gtest.h>
#include <AHO/pipeline/layouts.hpp>
namespace {
TEST(AhoLayoutsTest, RuntimeAndTemplateBindingLookupsAgreeOnResourceContracts) {
    using namespace aho::pipeline;
    const auto matrix = getBindingPoint(ResourceName::VPMatrixUBO);
    constexpr auto static_matrix = getBindingPoint<ResourceName::VPMatrixUBO>();
    static_assert(static_matrix.bindingType == vsl::graphic_resource::Type::UniformBuffer);
    static_assert(static_matrix.shaderType == vsl::ShaderFlag::Vertex && static_matrix.amount == 1);
    EXPECT_EQ(matrix.bindingType, vsl::graphic_resource::Type::UniformBuffer);
    EXPECT_EQ(matrix.shaderType, vsl::ShaderFlag::Vertex); EXPECT_EQ(matrix.amount, 1u);
    EXPECT_EQ(matrix.binding, static_matrix.binding);
    const auto texture = getBindingPoint(ResourceName::Texture);
    constexpr auto static_texture = getBindingPoint<ResourceName::Texture>();
    static_assert(static_texture.bindingType == vsl::graphic_resource::Type::CombinedImageSampler);
    static_assert(static_texture.shaderType == vsl::ShaderFlag::Fragment && static_texture.amount == 1);
    EXPECT_EQ(texture.bindingType, vsl::graphic_resource::Type::CombinedImageSampler);
    EXPECT_EQ(texture.shaderType, vsl::ShaderFlag::Fragment); EXPECT_EQ(texture.amount, 1u);
    EXPECT_EQ(texture.binding, static_texture.binding);
    EXPECT_THROW(getBindingPoint(static_cast<ResourceName>(999)), std::runtime_error);
}
}
#endif
