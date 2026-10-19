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

namespace {
    class PushRangeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        void SetUp() override { require_validation = true; VulkanIntegrationTest::SetUp(); }
    };
    vk::PushConstantRange range_for(vsl::PipelineLayoutEditAccessor layout, vk::ShaderStageFlagBits stage) {
        const auto& info = layout._data->info->pipelineLayout;
        unsigned found = 0;
        vk::PushConstantRange result{};
        for (unsigned i = 0; i < info.pushConstantRangeCount; ++i)
            if (info.pPushConstantRanges[i].stageFlags & stage) { result = info.pPushConstantRanges[i]; ++found; }
        EXPECT_EQ(found, 1u);
        return result;
    }
    TEST_F(PushRangeVulkanIntegrationTest, DifferentStagesCanUseTheSameByteOffsets) {
        auto layout = make_layout_state();
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::Vertex, 16, 8}, {vsl::ShaderFlag::Fragment, 16, 8}};
        constants.injection(layout);
        for (const auto stage : {vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment}) {
            const auto range = range_for(layout, stage);
            EXPECT_EQ(range.offset, 16u);
            EXPECT_EQ(range.size, 8u);
        }
    }
    TEST_F(PushRangeVulkanIntegrationTest, OverlappingAndUnorderedDefinitionsCoverTheirUnionOnlyOnce) {
        auto layout = make_layout_state();
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::Vertex, 16, 8}, {vsl::ShaderFlag::Vertex, 8, 16},
                                 {vsl::ShaderFlag::Vertex, 12, 4}, {vsl::ShaderFlag::Vertex, 32, 4}};
        constants.injection(layout);
        const auto range = range_for(layout, vk::ShaderStageFlagBits::eVertex);
        EXPECT_EQ(range.offset, 8u);
        EXPECT_EQ(range.size, 28u);
    }
    TEST_F(PushRangeVulkanIntegrationTest, OverlappingStageMasksDoNotDuplicateAStage) {
        auto layout = make_layout_state();
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::Vertex | vsl::ShaderFlag::Fragment, 4, 4},
                                 {vsl::ShaderFlag::Fragment, 12, 4}};
        constants.injection(layout);
        const auto vertex = range_for(layout, vk::ShaderStageFlagBits::eVertex);
        const auto fragment = range_for(layout, vk::ShaderStageFlagBits::eFragment);
        EXPECT_EQ(vertex.offset, 4u);
        EXPECT_EQ(vertex.size, 4u);
        EXPECT_EQ(fragment.offset, 4u);
        EXPECT_EQ(fragment.size, 12u);
        const auto& info = layout._data->info->pipelineLayout;
        for (unsigned i = 0; i < info.pushConstantRangeCount; ++i)
            for (unsigned j = i + 1; j < info.pushConstantRangeCount; ++j)
                ASSERT_FALSE(static_cast<bool>(info.pPushConstantRanges[i].stageFlags & info.pPushConstantRanges[j].stageFlags));
        const auto handle = device._data->device.createPipelineLayoutUnique(layout._data->info->pipelineLayout);
        EXPECT_TRUE(static_cast<bool>(handle));
    }
    TEST_F(PushRangeVulkanIntegrationTest, InvalidDefinitionsLeavePreviouslyInjectedRangesUnchanged) {
        auto layout = make_layout_state();
        pl::PushConstants valid;
        valid.definitions = {{vsl::ShaderFlag::Compute, 8, 8}};
        valid.injection(layout);
        const auto old = layout._data->info->pipelineLayout;
        const auto limit = physical._data->device.getProperties().limits.maxPushConstantsSize;
        for (const pl::PushContentDefinition definition : {
                pl::PushContentDefinition{vsl::ShaderFlag::Compute, 2, 4},
                {vsl::ShaderFlag::Compute, 0, 0}, {vsl::ShaderFlag::Compute, 0, 6},
                {vsl::ShaderFlag::Compute, UINT32_MAX - 3, 8},
                {vsl::ShaderFlag::Compute, 0, limit + 4}, {vsl::ShaderFlag::Error, 0, 4}}) {
            pl::PushConstants invalid;
            invalid.definitions = {definition};
            EXPECT_THROW(invalid.injection(layout), std::invalid_argument);
            const auto& current = layout._data->info->pipelineLayout;
            EXPECT_EQ(current.pushConstantRangeCount, old.pushConstantRangeCount);
            EXPECT_EQ(current.pPushConstantRanges, old.pPushConstantRanges);
            if (current.pushConstantRangeCount == 1) {
                EXPECT_EQ(current.pPushConstantRanges[0].offset, 8u);
                EXPECT_EQ(current.pPushConstantRanges[0].size, 8u);
            }
        }
    }
    TEST_F(PushRangeVulkanIntegrationTest, EmptyAndRepeatedInjectionReplaceRangesWithoutChangingOtherStages) {
        auto layout = make_layout_state();
        pl::ColorBlend blend;
        blend.injection(layout);
        const auto* colors = layout._data->info->colorBlend.pAttachments;
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::Compute, 12, 4}};
        constants.injection(layout);
        constants.definitions.clear();
        constants.injection(layout);
        EXPECT_EQ(layout._data->info->pipelineLayout.pushConstantRangeCount, 0u);
        EXPECT_EQ(layout._data->info->pipelineLayout.pPushConstantRanges, nullptr);
        EXPECT_EQ(layout._data->info->colorBlend.pAttachments, colors);
        constants.definitions = {{vsl::ShaderFlag::Compute, 4, 8}};
        constants.injection(layout);
        const auto range = range_for(layout, vk::ShaderStageFlagBits::eCompute);
        EXPECT_EQ(range.offset, 4u);
        EXPECT_EQ(range.size, 8u);
    }
} // namespace

TEST_F(PushRangeVulkanIntegrationTest, SubwordDefinitionsCanFormAnAlignedSharedRange) {
    auto layout = make_layout_state();
    const std::uint8_t first = 1, second = 2, third = 3, fourth = 4;
    const auto fields = pl::PushConstants::MakeDefinitions<vsl::ShaderFlag::Compute>(first, second, third, fourth);
    pl::PushConstants constants;
    constants.definitions.assign(fields.begin(), fields.end());
    constants.injection(layout);
    const auto range = range_for(layout, vk::ShaderStageFlagBits::eCompute);
    EXPECT_EQ(range.offset, 0u);
    EXPECT_EQ(range.size, 4u);
}

TEST_F(PushRangeVulkanIntegrationTest, RangeMayEndExactlyAtThePhysicalDeviceLimit) {
    auto layout = make_layout_state();
    const auto limit = physical._data->device.getProperties().limits.maxPushConstantsSize;
    ASSERT_GE(limit, 4u);
    pl::PushConstants constants;
    constants.definitions = {{vsl::ShaderFlag::Compute, limit - 4, 4}};
    constants.injection(layout);
    const auto range = range_for(layout, vk::ShaderStageFlagBits::eCompute);
    EXPECT_EQ(range.offset, limit - 4);
    EXPECT_EQ(range.size, 4u);
    const auto native = device._data->device.createPipelineLayoutUnique(layout._data->info->pipelineLayout);
    EXPECT_TRUE(static_cast<bool>(native));
}
