#include "support/vulkan_fixture.hpp"
#include "support/compute_fixture.hpp"
#include <VSL/lifetime/resource_arena.hpp>

namespace {
    class PipelineLayoutVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    class PipelineLayoutArenaComputeVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    bool PipelineLayoutVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, CopyOwnsItsVertexAndBlendStorage) {
        vsl::pipeline_layout::VertexInput input;
        input.add(vsl::data_format::FLOAT_RG);
        vsl::PipelineLayout original(device, input, vsl::pipeline_layout::ColorBlend{});
        auto copy = original.copy();
        EXPECT_NE(vsl_test::inspect_layout(copy).vertex_input.pVertexBindingDescriptions,
                  vsl_test::inspect_layout(original).vertex_input.pVertexBindingDescriptions);
        EXPECT_NE(vsl_test::inspect_layout(copy).vertex_input.pVertexAttributeDescriptions,
                  vsl_test::inspect_layout(original).vertex_input.pVertexAttributeDescriptions);
        EXPECT_NE(vsl_test::inspect_layout(copy).color_blend.pAttachments, vsl_test::inspect_layout(original).color_blend.pAttachments);
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
        EXPECT_NE(&copy.recipe(), &original.recipe());
        EXPECT_EQ(copy.data_->device, original.data_->device);
        auto shared = original;
        EXPECT_EQ(shared.data_, original.data_);
        const auto snapshot = copy.data_.get();
        auto frozen = std::move(copy).freeze();
        EXPECT_EQ(frozen.data_.get(), snapshot);
        EXPECT_FALSE(copy.data_);
        EXPECT_TRUE(frozen.has_recipe());
        EXPECT_THROW(copy.copy(), std::logic_error);
        EXPECT_THROW(copy.add(), std::logic_error);
        EXPECT_TRUE(frozen.data_->pipeline_layout);
        EXPECT_TRUE(original.has_recipe());
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, AddsStateAndCreatesUsableLayoutHandle) {
        vsl::PipelineLayout layout(device);
        auto added = layout.add(vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT));
        EXPECT_EQ(added.data_, layout.data_);
        EXPECT_TRUE(layout.data_->pipeline_layout);
        EXPECT_EQ(layout.recipe().rasterization.cullMode, vk::CullModeFlagBits::eFront);
    }
} // namespace

namespace {
    namespace t = vsl::typed;
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    namespace pl = vsl::pipeline_layout;
    constexpr std::array<t::ResourceRequest, 3> LAYOUT_REQUESTS{{{{1}, s::ResourceKind::PIPELINE_LAYOUT},
                                                                 {{2}, s::ResourceKind::PIPELINE_LAYOUT},
                                                                 {{3}, s::ResourceKind::DESCRIPTOR_LAYOUT}}};
    constexpr t::ResourcePlan<3> LAYOUT_PLAN(LAYOUT_REQUESTS);
    const vsl::DescriptorLayoutCreateInfo STORAGE_LAYOUT{{{0, vsl::graphic_resource::Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1},
                                                          {3, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE, 2}}};

    TEST_F(PipelineLayoutVulkanIntegrationTest, PlacedLayoutsUseDistinctSlotsAndOneRuntimeOwner) {
        const std::array<l::ResourceInit, 3> inputs{
            l::PipelineLayoutInit{{t::ResourceId{3}}, {{vsl::ShaderFlag::COMPUTE, 0, 4}}},
            l::PipelineLayoutInit{{t::ResourceId{3}}, {}}, STORAGE_LAYOUT};
        l::ResourceArena arena(device, LAYOUT_PLAN, inputs);
        const auto first = arena.pipeline_layout({1}), second = arena.pipeline_layout({2});
        EXPECT_FALSE(first.has_recipe());
        EXPECT_THROW(first.recipe(), std::logic_error);
        EXPECT_NE(first.data_->pipeline_layout, second.data_->pipeline_layout);
        const auto base = reinterpret_cast<std::uintptr_t>(first.data_.get());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second.data_.get()) - base, LAYOUT_PLAN.placement({2}).offset);
        EXPECT_EQ(base % s::PIPELINE_LAYOUT_IMPL_ALIGNMENT, 0u);
        EXPECT_FALSE(first.data_.owner_before(second.data_));
        EXPECT_FALSE(second.data_.owner_before(first.data_));
        EXPECT_TRUE(first.owner_ref().managed());
        EXPECT_EQ(first.get_device().data_, device.data_);
        EXPECT_EQ(first.data_->push_ranges.size(), 1u);
        EXPECT_THROW(arena.pipeline_layout({3}), std::invalid_argument);
        EXPECT_THROW(arena.pipeline_layout({404}), std::out_of_range);
        // Exact structural comparison accepts an independently allocated, identical layout.
        auto points = STORAGE_LAYOUT.bindings;
        std::reverse(points.begin(), points.end());
        vsl::graphic_resource::BindingLayout identical(device, points);
        auto composed = vsl::PipelineLayout::FromRuntime(first, pl::ResourceBinding{{identical}},
                                                         pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
        EXPECT_EQ(composed.data_, first.data_);
        auto frozen = std::move(composed).freeze();
        EXPECT_EQ(frozen.data_, first.data_);
        EXPECT_FALSE(frozen.runtime().has_recipe());
        EXPECT_THROW(vsl::PipelineLayout::FromRuntime(first, pl::ResourceBinding{{identical}}), std::invalid_argument);
        EXPECT_THROW(vsl::PipelineLayout::FromRuntime(second), std::invalid_argument);
        vsl::graphic_resource::BindingLayout wrong(device, vsl::graphic_resource::Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE);
        EXPECT_THROW(vsl::PipelineLayout::FromRuntime(second, pl::ResourceBinding{{wrong}}), std::invalid_argument);
        constexpr std::array<t::ResourceRequest, 1> direct_requests{{{{1}, s::ResourceKind::PIPELINE_LAYOUT}}};
        constexpr t::ResourcePlan<1> direct_plan(direct_requests);
        for (const bool borrowed : {false, true}) {
            auto invalid = identical;
            invalid.data_ = borrowed ? std::shared_ptr<vsl::impl::ResourceBindingLayoutImpl>(std::shared_ptr<void>{}, identical.data_.get()) : nullptr;
            const std::array<l::ResourceInit, 1> invalid_inputs{l::PipelineLayoutInit{{invalid}, {}}};
            EXPECT_THROW(l::ResourceArena(device, direct_plan, invalid_inputs), std::invalid_argument);
        }
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, FailedDependentConstructionReleasesTheRegionAndPreservesImports) {
        vsl::storage::ArenaMemory memory(LAYOUT_PLAN.bytes(), LAYOUT_PLAN.alignment());
        auto inputs = std::array<l::ResourceInit, 3>{l::PipelineLayoutInit{{t::ResourceId{3}}, {}},
                                                     l::PipelineLayoutInit{{t::ResourceId{3}}, {{vsl::ShaderFlag::COMPUTE, 2, 4}}}, STORAGE_LAYOUT};
        for (int i = 0; i < 3; ++i)
            EXPECT_THROW(l::ResourceArena(device, LAYOUT_PLAN, inputs, memory), std::invalid_argument);
        inputs[1] = l::PipelineLayoutInit{{t::ResourceId{3}}, {{vsl::ShaderFlag::COMPUTE, 0, 4}}};
        {
            l::ResourceArena arena(device, LAYOUT_PLAN, inputs, memory);
            EXPECT_TRUE(arena.pipeline_layout({1}).data_->pipeline_layout);
            EXPECT_TRUE(arena.pipeline_layout({2}).data_->pipeline_layout);
        }
        auto invalid = inputs;
        invalid[0] = l::PipelineLayoutInit{{t::ResourceId{99}}, {}};
        EXPECT_THROW(l::ResourceArena(device, LAYOUT_PLAN, invalid, memory), std::out_of_range);
        EXPECT_NO_THROW(l::ResourceArena(device, LAYOUT_PLAN, inputs, memory));
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, RuntimeImportDoesNotRetainTheImportersOrTheSourceRecipe) {
        std::weak_ptr<void> source_owner, importer_owner, recipe_binding;
        vsl::PipelineLayoutAccessor kept;
        {
            vsl::graphic_resource::BindingLayout binding(device, STORAGE_LAYOUT.bindings);
            recipe_binding = binding.data_;
            vsl::PipelineLayout source(device, pl::ResourceBinding{{binding}});
            source_owner = source.data_;
            constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::PIPELINE_LAYOUT, true}}};
            constexpr t::ResourcePlan<1> plan(requests);
            {
                const std::array<l::ResourceInit, 1> inputs{vsl::PipelineLayoutAccessor(source)};
                l::ResourceArena importer(device, plan, inputs);
                importer_owner = importer.owner();
                kept = importer.pipeline_layout({1});
                EXPECT_FALSE(kept.has_recipe());
                EXPECT_EQ(kept.data_, source.data_);
                EXPECT_FALSE(kept.data_.owner_before(source.data_));
                EXPECT_FALSE(source.data_.owner_before(kept.data_));
            }
            EXPECT_TRUE(importer_owner.expired());
        }
        EXPECT_TRUE(recipe_binding.expired());
        EXPECT_FALSE(source_owner.expired());
        kept = {};
        EXPECT_TRUE(source_owner.expired());
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, ReplacingBindingsDropsUnusedRecipeOwners) {
        vsl::PipelineLayout layout(device);
        std::weak_ptr<void> previous;
        {
            vsl::graphic_resource::BindingLayout binding(device, STORAGE_LAYOUT.bindings);
            previous = binding.data_;
            pl::ResourceBinding{{binding}}.injection(layout);
        }
        EXPECT_FALSE(previous.expired());
        pl::ResourceBinding{{}}.injection(layout);
        EXPECT_TRUE(previous.expired());
        EXPECT_TRUE(layout.recipe().sources.empty());
        EXPECT_TRUE(layout.recipe().binding_layouts.empty());
    }
    TEST_F(PipelineLayoutVulkanIntegrationTest, ReplacementPublishesAllOwnersBeforeAnOldOwnerCanReenter) {
        for (const bool move : {false, true}) {
            vsl::PipelineLayout original(device), replacement(device), next(device, pl::Rasterization(pl::CullingMode::FRONT));
            std::optional<vsl::PipelineLayout> current;
            bool callback = false;
            auto alias = std::shared_ptr<vsl::impl::PipelineLayoutImpl>(original.data_.get(),
                                                                        [owner = original.runtime(), &current, &next, &replacement, &callback](auto*) {
                                                                            callback = true;
                                                                            ASSERT_TRUE(current.has_value());
                                                                            EXPECT_EQ(current->data_, next.data_);
                                                                            EXPECT_EQ(&current->recipe(), &next.recipe());
                                                                            EXPECT_EQ(&current->edit_recipe(), &current->recipe());
                                                                            *current = replacement;
                                                                        });
            current.emplace(vsl::PipelineLayout::FromRuntime(vsl::PipelineLayoutAccessor(alias)));
            alias.reset();
            if (move) {
                auto value = next;
                *current = std::move(value);
            } else {
                *current = next;
            }
            EXPECT_TRUE(callback);
            EXPECT_EQ(current->data_, replacement.data_);
            EXPECT_EQ(&current->recipe(), &replacement.recipe());
            EXPECT_EQ(&current->edit_recipe(), &current->recipe());
        }
    }
    TEST_F(PipelineLayoutArenaComputeVulkanIntegrationTest, RuntimePipelineAndSavedPushDoNotKeepTheShaderRecipe) {
        pipeline.reset();
        layout.reset();
        const std::weak_ptr<void> shader_owner = shader->data_;
        std::weak_ptr<void> native_owner, unused_binding;
        std::optional<vsl::command::PushConstant> push;
        std::uint32_t value = 617;
        {
            constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, s::ResourceKind::PIPELINE_LAYOUT}}};
            constexpr t::ResourcePlan<1> plan(requests);
            vsl::graphic_resource::BindingLayout extra(device, std::vector<vsl::graphic_resource::BindingPoint>{});
            unused_binding = extra.data_;
            const std::array<l::ResourceInit, 1> inputs{l::PipelineLayoutInit{{*binding, extra}, {{vsl::ShaderFlag::COMPUTE, 0, 4}}}};
            l::ResourceArena arena(device, plan, inputs);
            native_owner = arena.owner();
            auto composed = vsl::PipelineLayout::FromRuntime(arena.pipeline_layout({1}), pl::ShaderGroup("compute", {*shader}),
                                                             pl::ResourceBinding{{*binding, extra}}, pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
            auto frozen = std::move(composed).freeze();
            pipeline.emplace(frozen);
            push.emplace(frozen, vsl::ShaderFlag::COMPUTE, value);
            EXPECT_FALSE(push->layout.has_recipe());
            shader.reset();
            EXPECT_FALSE(shader_owner.expired());
        }
        EXPECT_TRUE(shader_owner.expired());
        EXPECT_TRUE(unused_binding.expired());
        EXPECT_FALSE(native_owner.expired());
        auto manager = make_manager();
        vsl::Completion completion;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                auto stream = phase << *pipeline;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                       << *push << vsl::command::Dispatch(COUNT);
                const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
                auto buffer = manager.get_buffer();
                buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            pipeline.reset();
            push.reset();
            EXPECT_FALSE(native_owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            completion = phase.completion();
            ASSERT_TRUE(completion.wait());
            EXPECT_FALSE(native_owner.expired());
        }
        {
            auto recycle = manager.start_phase<vsl::ComputePhase>();
            ASSERT_EQ(recycle.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(recycle.completion().wait());
        }
        EXPECT_TRUE(native_owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::COMPLETE);
        std::array<std::uint32_t, COUNT> actual{};
        output->read_into(std::as_writable_bytes(std::span(actual)));
        ExpectSequence(actual, value);
    }
    TEST_F(PipelineLayoutArenaComputeVulkanIntegrationTest, CancellationReleasesRuntimeLayoutWithoutKeepingRecipeSources) {
        auto manager = make_manager();
        pipeline.reset();
        layout.reset();
        const std::weak_ptr<void> shader_owner = shader->data_;
        std::weak_ptr<void> native_owner;
        std::uint32_t value = 271;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                vsl::PipelineLayout recipe(device, pl::ShaderGroup("compute", {*shader}), pl::ResourceBinding{{*binding}},
                                           pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
                native_owner = recipe.data_;
                vsl::ComputePipeline selected(recipe);
                phase << selected << vsl::command::PushConstant(recipe, vsl::ShaderFlag::COMPUTE, value);
                shader.reset();
            }
            EXPECT_TRUE(shader_owner.expired());
            EXPECT_FALSE(native_owner.expired());
            phase.cancel();
            EXPECT_FALSE(native_owner.expired());
        }
        EXPECT_TRUE(native_owner.expired());
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
    template<typename T>
    concept CanBorrowTemporaryRecipe = requires(T&& layout) { std::move(layout).recipe(); };
    template<typename T>
    concept CanEditTemporaryRecipe = requires(T&& layout) { std::move(layout).edit_recipe(); };
    static_assert(!CanBorrowTemporaryRecipe<vsl::PipelineLayout>);
    static_assert(!CanBorrowTemporaryRecipe<vsl::FrozenPipelineLayout>);
    static_assert(!CanEditTemporaryRecipe<vsl::PipelineLayout>);

    TEST(PipelineLayoutTest, MissingDeviceAndRuntimeAreRejectedBeforeNativeUse) {
        vsl::LogicalDeviceAccessor missing;
        EXPECT_THROW((vsl::PipelineLayout{missing}), std::invalid_argument);
        missing.data_ = std::make_shared<vsl::impl::LogicalDeviceImpl>();
        EXPECT_THROW((vsl::PipelineLayout{missing}), std::invalid_argument);
        const vsl::PipelineLayoutAccessor empty;
        EXPECT_THROW(empty.get_device(), std::invalid_argument);
        EXPECT_THROW(empty.owner_ref(), std::invalid_argument);
        EXPECT_THROW(empty.recipe(), std::logic_error);
        EXPECT_THROW(vsl::PipelineLayout::FromRuntime(empty), std::invalid_argument);
    }

    TEST_F(PipelineLayoutVulkanIntegrationTest, FreezeSeparatesRemainingEditableAliases) {
        vsl::PipelineLayout original(device);
        auto alias = original;
        auto frozen = std::move(original).freeze();
        EXPECT_EQ(frozen.data_, alias.data_);
        EXPECT_NE(&frozen.recipe(), &alias.recipe());
        vsl::pipeline_layout::Rasterization(vsl::pipeline_layout::CullingMode::FRONT).injection(alias);
        EXPECT_NE(frozen.recipe().rasterization.cullMode, alias.recipe().rasterization.cullMode);
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
            EXPECT_NE(copied->recipe().binding_layouts.data(), original.recipe().binding_layouts.data());
            EXPECT_NE(copied->recipe().push_ranges.data(), original.recipe().push_ranges.data());
        }
        EXPECT_FALSE(weak_binding.expired());
        ASSERT_EQ(copied->data_->push_ranges.size(), 1u);
        EXPECT_EQ(copied->data_->push_ranges[0].size, 4u);
        copied.reset();
        EXPECT_TRUE(weak_binding.expired());
    }
} // namespace
