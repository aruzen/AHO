#include "support/vulkan_fixture.hpp"

namespace {
    class PipelineLayoutVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool PipelineLayoutVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, CopyOwnsItsVertexAndBlendStorage) {
        vsl::pipeline_layout::VertexInput input;
        input.add(vsl::data_format::FLOAT_RG);
        vsl::PipelineLayout original(device, input, vsl::pipeline_layout::ColorBlend{});
        auto copy = original.copy();
        EXPECT_NE(copy.data_->info->vertex_input.pVertexBindingDescriptions,
                  original.data_->info->vertex_input.pVertexBindingDescriptions);
        EXPECT_NE(copy.data_->info->vertex_input.pVertexAttributeDescriptions,
                  original.data_->info->vertex_input.pVertexAttributeDescriptions);
        EXPECT_NE(copy.data_->info->color_blend.pAttachments, original.data_->info->color_blend.pAttachments);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, EditingAnAliasPreservesTheExistingSnapshot) {
        vsl::PipelineLayout original(device);
        auto alias = original;
        const auto handle = original.data_->pipeline_layout;
        alias.add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT));
        EXPECT_NE(alias.data_, original.data_);
        EXPECT_EQ(original.data_->pipeline_layout, handle);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, CreatesIndependentCopyAndFreezesEditableState) {
        vsl::PipelineLayout original(device);
        auto copy = original.copy();
        EXPECT_NE(copy.data_->pipeline_layout, original.data_->pipeline_layout);
        EXPECT_NE(copy.data_->info, original.data_->info);
        EXPECT_EQ(copy.data_->device, original.data_->device);
        auto shared = original;
        EXPECT_EQ(shared.data_, original.data_);
        const auto snapshot = copy.data_.get();
        auto frozen = std::move(copy).freeze();
        EXPECT_EQ(frozen.data_.get(), snapshot);
        EXPECT_FALSE(copy.data_);
        EXPECT_NE(frozen.data_->info, nullptr);
        EXPECT_THROW(copy.copy(), std::logic_error);
        EXPECT_THROW(copy.add(), std::logic_error);
        EXPECT_TRUE(frozen.data_->pipeline_layout);
        EXPECT_NE(original.data_->info, nullptr);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, AddsStateAndCreatesUsableLayoutHandle) {
        vsl::PipelineLayout layout(device);
        auto added = layout.add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT));
        EXPECT_EQ(added.data_, layout.data_);
        EXPECT_TRUE(layout.data_->pipeline_layout);
        EXPECT_EQ(layout.data_->info->rasterization.cullMode, vk::CullModeFlagBits::eFront);
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
        EXPECT_NE(frozen.data_, alias.data_);
        vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT).injection(alias);
        EXPECT_NE(frozen.data_->info->rasterization.cullMode, alias.data_->info->rasterization.cullMode);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, FailedAdditionPreservesTheOriginalLayout) {
        struct FailingStage {
            void injection(vsl::PipelineLayoutEditAccessor) { throw std::runtime_error("stage failed"); }
        };
        vsl::PipelineLayout layout(device);
        const auto snapshot = layout.data_;
        EXPECT_THROW(layout.add(FailingStage{}), std::runtime_error);
        EXPECT_EQ(layout.data_, snapshot);
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, CopyOwnsDescriptorLayoutsAndPushConstantRanges) {
        std::optional<vsl::PipelineLayout> copied;
        std::weak_ptr<vsl::impl::ResourceBindingLayoutImpl> weak_binding;
        {
            vsl::graphic_resource::BindingLayout binding(device, vsl::graphic_resource::Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
            weak_binding = binding.data_;
            namespace pl = vsl::pipeline_layout;
            vsl::PipelineLayout original(device, pl::ResourceBinding{{binding}},
                                         pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
            copied = original.copy();
            EXPECT_NE(copied->data_->info->pipeline_layout.pSetLayouts, original.data_->info->pipeline_layout.pSetLayouts);
            EXPECT_NE(copied->data_->info->pipeline_layout.pPushConstantRanges, original.data_->info->pipeline_layout.pPushConstantRanges);
        }
        EXPECT_FALSE(weak_binding.expired());
        ASSERT_EQ(copied->data_->info->pipeline_layout.pushConstantRangeCount, 1u);
        EXPECT_EQ(copied->data_->info->pipeline_layout.pPushConstantRanges[0].size, 4u);
        copied.reset();
        EXPECT_TRUE(weak_binding.expired());
    }
} // namespace
