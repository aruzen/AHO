#include "support/vulkan_fixture.hpp"
#include <array>

namespace {
    namespace pl = vsl::pipeline_layout;
    using PushContentsVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST(PushContentsTest, DefaultAndSingleDefinitionRetainStageSizeAndOffset) {
        pl::PushConstants empty;
        EXPECT_TRUE(empty.definitions.empty());
        std::uint32_t value = 0;
        const auto definitions = pl::PushConstants::MakeDefinitions<vsl::ShaderFlag::Vertex>(value);
        ASSERT_EQ(definitions.size(), 1u);
        EXPECT_EQ(definitions[0].targetShader, vsl::ShaderFlag::Vertex);
        EXPECT_EQ(definitions[0].offset, 0u);
        EXPECT_EQ(definitions[0].size, sizeof(value));
        pl::PushConstants constants(vsl::ShaderFlag::Vertex, {definitions[0]});
        ASSERT_EQ(constants.definitions.size(), 1u);
        EXPECT_EQ(constants.definitions[0].size, sizeof(value));
    }
    TEST_F(PushContentsVulkanIntegrationTest, InjectsOneContiguousRange) {
        auto layout = make_layout_state();
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::Vertex, 0, 16}, {vsl::ShaderFlag::Vertex, 16, 4}};
        constants.injection(layout);
        const auto& info = layout._data->info->pipelineLayout;
        ASSERT_EQ(info.pushConstantRangeCount, 1u);
        EXPECT_EQ(info.pPushConstantRanges[0].stageFlags, vk::ShaderStageFlagBits::eVertex);
        EXPECT_EQ(info.pPushConstantRanges[0].offset, 0u);
        EXPECT_EQ(info.pPushConstantRanges[0].size, 20u);
    }
    TEST(PushContentsTest, MultipleDefinitionsHaveConsecutiveOffsets) {
        std::uint32_t first = 0;
        std::array<std::uint32_t, 2> second{};
        const auto definitions = pl::PushConstants::MakeDefinitions<vsl::ShaderFlag::Compute>(first, second);
        EXPECT_EQ(definitions[0].offset, 0u);
        EXPECT_EQ(definitions[1].offset, sizeof(first));
        EXPECT_EQ(definitions[1].size, sizeof(second));
        for (const auto& definition : definitions)
            EXPECT_EQ(definition.targetShader, vsl::ShaderFlag::Compute);
    }
    TEST(PushContentsTest, MixedSizesAccumulateOffsetsInArgumentOrder) {
        const std::array<std::uint32_t, 3> first{1, 2, 3};
        const float second = 4.0f;
        const std::array<std::uint32_t, 2> third{5, 6};
        constexpr auto stage = vsl::ShaderFlag::Vertex;
        constexpr auto definitions = pl::PushConstants::MakeDefinitions<stage>(first, second, third);
        static_assert(definitions.size() == 3);
        static_assert(definitions[0].offset == 0);
        static_assert(definitions[1].offset == 12);
        static_assert(definitions[2].offset == 16);
        EXPECT_EQ(definitions[0].size, 12u);
        EXPECT_EQ(definitions[1].size, 4u);
        EXPECT_EQ(definitions[2].size, 8u);
        for (const auto& definition : definitions)
            EXPECT_EQ(definition.targetShader, stage);
        EXPECT_EQ(first, (std::array<std::uint32_t, 3>{1, 2, 3}));
        EXPECT_FLOAT_EQ(second, 4.0f);
        EXPECT_EQ(third, (std::array<std::uint32_t, 2>{5, 6}));
    }
    TEST(PushContentsTest, EmptyArgumentsProduceAnEmptyDefinitionArray) {
        constexpr auto definitions = pl::PushConstants::MakeDefinitions<vsl::ShaderFlag::Compute>();
        static_assert(definitions.empty());
        EXPECT_TRUE(definitions.empty());
    }
} // namespace
