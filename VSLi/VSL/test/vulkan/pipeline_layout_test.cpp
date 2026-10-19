#include "support/vulkan_fixture.hpp"

namespace {
    class PipelineLayoutVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(PipelineLayoutVulkanIntegrationTest, CopyOwnsItsVertexAndBlendStorage) {
        vsl::pipeline_layout::VertexInput input;
        input.add(vsl::data_format::FloatRG);
        vsl::PipelineLayout original(device, input, vsl::pipeline_layout::ColorBlend{});
        auto copy = original.copy();
        EXPECT_NE(copy._data->info->vertexInput.pVertexBindingDescriptions,
                  original._data->info->vertexInput.pVertexBindingDescriptions);
        EXPECT_NE(copy._data->info->vertexInput.pVertexAttributeDescriptions,
                  original._data->info->vertexInput.pVertexAttributeDescriptions);
        EXPECT_NE(copy._data->info->colorBlend.pAttachments, original._data->info->colorBlend.pAttachments);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, EditingAnAliasPreservesTheExistingSnapshot) {
        vsl::PipelineLayout original(device);
        auto alias = original;
        const auto handle = original._data->pipelineLayout;
        alias.add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::Front));
        EXPECT_NE(alias._data, original._data);
        EXPECT_EQ(original._data->pipelineLayout, handle);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, CreatesIndependentCopyAndFreezesEditableState) {
        vsl::PipelineLayout original(device);
        auto copy = original.copy();
        EXPECT_NE(copy._data->pipelineLayout, original._data->pipelineLayout);
        EXPECT_NE(copy._data->info, original._data->info);
        EXPECT_EQ(copy._data->device, original._data->device);
        auto shared = original;
        EXPECT_EQ(shared._data, original._data);
        const auto snapshot = copy._data.get();
        auto frozen = std::move(copy).freeze();
        EXPECT_EQ(frozen._data.get(), snapshot);
        EXPECT_FALSE(copy._data);
        EXPECT_NE(frozen._data->info, nullptr);
        EXPECT_THROW(copy.copy(), std::logic_error);
        EXPECT_THROW(copy.add(), std::logic_error);
        EXPECT_TRUE(frozen._data->pipelineLayout);
        EXPECT_NE(original._data->info, nullptr);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, AddsStateAndCreatesUsableLayoutHandle) {
        vsl::PipelineLayout layout(device);
        auto added = layout.add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::Front));
        EXPECT_EQ(added._data, layout._data);
        EXPECT_TRUE(layout._data->pipelineLayout);
        EXPECT_EQ(layout._data->info->rasterization.cullMode, vk::CullModeFlagBits::eFront);
    }
} // namespace

namespace {
    template<typename T>
    concept CanEditLayout = requires(T& layout) { layout.add(); };
    template<typename T>
    concept CanInjectRasterization = requires(T layout, vsl::pipeline_layout::Rasterization stage) { stage.injection(layout); };
    template<typename T>
    concept CanFreezeLvalue = requires(T& layout) { layout.freeze(); };
    static_assert(CanEditLayout<vsl::PipelineLayout>);
    static_assert(!CanEditLayout<vsl::FrozenPipelineLayout>);
    static_assert(!CanInjectRasterization<vsl::FrozenPipelineLayout>);
    static_assert(!CanFreezeLvalue<vsl::PipelineLayout>);
    static_assert(!std::is_convertible_v<vsl::FrozenPipelineLayout, vsl::PipelineLayoutEditAccessor>);
    static_assert(std::same_as<decltype(std::declval<vsl::PipelineLayout&&>().freeze()), vsl::FrozenPipelineLayout>);

    TEST_F(PipelineLayoutVulkanIntegrationTest, FreezeSeparatesRemainingEditableAliases) {
        vsl::PipelineLayout original(device);
        auto alias = original;
        auto frozen = std::move(original).freeze();
        EXPECT_NE(frozen._data, alias._data);
        vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::Front).injection(alias);
        EXPECT_NE(frozen._data->info->rasterization.cullMode, alias._data->info->rasterization.cullMode);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, FailedAdditionPreservesTheOriginalLayout) {
        struct FailingStage {
            void injection(vsl::PipelineLayoutEditAccessor) { throw std::runtime_error("stage failed"); }
        };
        vsl::PipelineLayout layout(device);
        const auto snapshot = layout._data;
        EXPECT_THROW(layout.add(FailingStage{}), std::runtime_error);
        EXPECT_EQ(layout._data, snapshot);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, CopyOwnsDescriptorLayoutsAndPushConstantRanges) {
        std::optional<vsl::PipelineLayout> copied;
        std::weak_ptr<vsl::_impl::ResourceBindingLayout_impl> weak_binding;
        {
            vsl::graphic_resource::BindingLayout binding(device, vsl::graphic_resource::Type::StorageBuffer, vsl::ShaderFlag::Compute);
            weak_binding = binding._data;
            namespace pl = vsl::pipeline_layout;
            vsl::PipelineLayout original(device, pl::ResourceBinding{{binding}},
                                         pl::PushConstants(vsl::ShaderFlag::Compute, {{vsl::ShaderFlag::Compute, 0, 4}}));
            copied = original.copy();
            EXPECT_NE(copied->_data->info->pipelineLayout.pSetLayouts, original._data->info->pipelineLayout.pSetLayouts);
            EXPECT_NE(copied->_data->info->pipelineLayout.pPushConstantRanges, original._data->info->pipelineLayout.pPushConstantRanges);
        }
        EXPECT_FALSE(weak_binding.expired());
        ASSERT_EQ(copied->_data->info->pipelineLayout.pushConstantRangeCount, 1u);
        EXPECT_EQ(copied->_data->info->pipelineLayout.pPushConstantRanges[0].size, 4u);
        copied.reset();
        EXPECT_TRUE(weak_binding.expired());
    }
} // namespace
