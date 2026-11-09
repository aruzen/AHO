#include "support/vulkan_fixture.hpp"
#include "support/fixed_parent.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/shader_access.hpp>
#include <VSL/lifetime/pipeline_recipe.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::ShaderAccess;
    using VSL_NAMESPACE::lifetime::detail::ShaderGroupAccess;
} // namespace

namespace {
    using ShaderGroupVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(ShaderGroupVulkanIntegrationTest, MovedGroupsKeepSharedEditingAndRejectEveryEmptyEntry) {
        const vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup original("shared", {});
        auto alias = original;
        auto moved = std::move(original);
        EXPECT_FALSE(original);
        EXPECT_TRUE(alias);
        EXPECT_TRUE(moved);
        auto layout = make_layout_state();
        EXPECT_THROW(original.add_shader(vertex), std::logic_error);
        EXPECT_THROW(original.remove_shader("main"), std::logic_error);
        EXPECT_THROW(original.remove_shader(vertex), std::logic_error);
        EXPECT_THROW(original.injection(layout), std::logic_error);
        alias.add_shader(vertex);
        moved.injection(layout);
        ASSERT_EQ(layout.recipe().shaders.size(), 1u);
        EXPECT_EQ(layout.recipe().shaders.front(), ShaderAccess::Owned(vertex).get());
        moved.remove_shader(vertex);
        EXPECT_TRUE(ShaderGroupAccess::Data(alias)->shaders.empty());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, BothRemovalOverloadsPublishBeforeTheReleasedOwnerReentersTheGroup) {
        const vsl::Shader<vsl::ShaderType::VERTEX> first(device, vsl_test::shader_path("triangle.vert.spv"), "first");
        const vsl::Shader<vsl::ShaderType::FRAGMENT> second(device, vsl_test::shader_path("triangle.frag.spv"), "second");
        const vsl::Shader<vsl::ShaderType::VERTEX> third(device, vsl_test::shader_path("triangle.vert.spv"), "third");
        struct Context {
            vsl::ShaderAccessor source;
            std::shared_ptr<void> action;
        };
        for (const bool by_name : {true, false}) {
            std::optional<vsl::pipeline_layout::ShaderGroup> group;
            size_t observed = 0;
            bool appended = false;
            vsl_test::DeferredAction release([&] {
                observed = ShaderGroupAccess::Data(*group)->shaders.size();
                // 旧実装では削除途中なので、iteratorを壊す再入は行わず状態だけ観測する。
                if (observed == 1) {
                    group->add_shader(third);
                    appended = true;
                }
            });
            {
                auto context = std::make_shared<Context>(Context{first, release.owner()});
                const auto composite = ShaderAccess::FromShared(std::shared_ptr<vsl::impl::ShaderImpl>(context, ShaderAccess::Owned(first).get()));
                group.emplace("callback", std::initializer_list<vsl::ShaderAccessor>{composite, second});
            }
            if (by_name)
                group->remove_shader("first");
            else
                group->remove_shader(first);
            EXPECT_EQ(observed, 1u);
            EXPECT_TRUE(appended);
            const auto& shaders = ShaderGroupAccess::Data(*group)->shaders;
            EXPECT_EQ(shaders.size(), 2u);
            if (shaders.size() == 2) {
                EXPECT_EQ(shaders[0].get(), ShaderAccess::Owned(second).get());
                EXPECT_EQ(shaders[1].get(), ShaderAccess::Owned(third).get());
            }
        }
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, RecipeAndImportedShadersKeepCompositeProvenanceAndRejectTheOriginalArenaCycle) {
        namespace l = vsl::lifetime;
        namespace t = vsl::typed;
        using Access = l::detail::OwnerAccess;
        std::weak_ptr<void> observed_arena, observed_context;
        {
            const vsl::Shader<vsl::ShaderType::COMPUTE> source(device, vsl_test::shader_path("fill.comp.spv"));
            constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, vsl::storage::ResourceKind::SHADER}}};
            constexpr t::ResourcePlan<1> plan(requests);
            const std::array<l::ResourceInit, 1> inputs{vsl::ShaderCreateInfo{source.type(), {source.code().begin(), source.code().end()}}};
            l::ResourceArena arena(device, plan, inputs);
            observed_arena = arena.owner();
            auto context = vsl_test::FixedParentOwner::Create(arena.owner_ref());
            observed_context = context;
            const auto placed = arena.shader({1});
            const auto composite = ShaderAccess::FromOwned(context->node.ref(context).alias(ShaderAccess::Owned(placed).get()));
            vsl::pipeline_layout::ShaderGroup group("managed", {composite});
            group.add_shader(composite);
            l::PipelineRecipeDraft draft(device);
            draft.inject(group);
            const auto snapshot = draft.snapshot();
            const auto& recipe = snapshot.definition();
            ASSERT_EQ(recipe.shaders.size(), 2u);
            EXPECT_EQ(recipe.shader_sources[0], recipe.shader_sources[1]);
            constexpr std::array<t::ResourceRequest, 1> external_requests{{{{2}, vsl::storage::ResourceKind::SHADER, true}}};
            constexpr t::ResourcePlan<1> external_plan(external_requests);
            for (size_t i = 0; i < recipe.shaders.size(); ++i) {
                EXPECT_EQ(Access::Node(recipe.sources.at(recipe.shader_sources.at(i))), &context->node);
                const auto restored = l::detail::ShaderAccess::FromRecipe(recipe, i);
                EXPECT_EQ(Access::Node(restored.owner_ref()), &context->node);
                const std::array<l::ResourceInit, 1> external_inputs{restored};
                l::ResourceArena external(device, external_plan, external_inputs);
                EXPECT_EQ(Access::Node(external.shader({2}).owner_ref()), &context->node);
                vsl_test::expect_fixed_parent(external.owner_ref(), context->node.ref(context));
                vsl_test::expect_fixed_parent(external.owner_ref(), arena.owner_ref(), false);
            }
        }
        EXPECT_TRUE(observed_context.expired());
        EXPECT_TRUE(observed_arena.expired());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, InjectsInitialAndAppendedShadersInOrder) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"));
        vsl::pipeline_layout::ShaderGroup group("triangle", {vertex});
        group.add_shader(fragment);
        ASSERT_EQ(ShaderGroupAccess::Data(group)->shaders.size(), 2u);
        EXPECT_EQ(ShaderGroupAccess::Data(group)->name, "triangle");
        auto layout = make_layout_state();
        group.injection(layout);
        ASSERT_EQ(layout.recipe().shaders.size(), 2u);
        EXPECT_EQ(layout.recipe().shaders[0]->shader_module, ShaderAccess::Owned(vertex)->shader_module);
        EXPECT_EQ(layout.recipe().shaders[1]->shader_module, ShaderAccess::Owned(fragment)->shader_module);
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, RemovesMatchingShaderByNameAndHandle) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"),
                                                    "vertex_entry");
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"),
                                                        "fragment_entry");
        vsl::pipeline_layout::ShaderGroup group("triangle", {vertex, fragment});
        group.remove_shader("vertex_entry");
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders.size(), 1u);
        group.remove_shader(fragment);
        EXPECT_TRUE(ShaderGroupAccess::Data(group)->shaders.empty());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, EmptyGroupsAllowBothRemovalOverloads) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup group("empty", {});
        group.remove_shader("missing");
        group.remove_shader(vertex);
        EXPECT_TRUE(ShaderGroupAccess::Data(group)->shaders.empty());
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, NameRemovalErasesEveryMatchAndKeepsOtherEntriesInOrder) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"), "shared");
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"), "shared");
        vsl::Shader<vsl::ShaderType::VERTEX> other(device, vsl_test::shader_path("triangle.vert.spv"), "other");
        vsl::pipeline_layout::ShaderGroup group("duplicates", {vertex, fragment, other, vertex, other});
        group.remove_shader("shared");
        ASSERT_EQ(ShaderGroupAccess::Data(group)->shaders.size(), 2u);
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders[0], ShaderAccess::Owned(other));
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders[1], ShaderAccess::Owned(other));
        group.remove_shader("other");
        EXPECT_TRUE(ShaderGroupAccess::Data(group)->shaders.empty());
        group.remove_shader("other");
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, HandleRemovalKeepsDifferentModulesWithTheSameName) {
        vsl::Shader<vsl::ShaderType::VERTEX> first(device, vsl_test::shader_path("triangle.vert.spv"), "same");
        vsl::Shader<vsl::ShaderType::VERTEX> second(device, vsl_test::shader_path("triangle.vert.spv"), "same");
        vsl::pipeline_layout::ShaderGroup group("duplicates", {first, second, first, second});
        group.remove_shader(first);
        ASSERT_EQ(ShaderGroupAccess::Data(group)->shaders.size(), 2u);
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders[0], ShaderAccess::Owned(second));
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders[1], ShaderAccess::Owned(second));
        group.remove_shader(second);
        EXPECT_TRUE(ShaderGroupAccess::Data(group)->shaders.empty());
        group.remove_shader(second);
    }
    TEST_F(ShaderGroupVulkanIntegrationTest, MissingMatchesPreserveOrderAndInjectedStages) {
        vsl::Shader<vsl::ShaderType::VERTEX> vertex(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::Shader<vsl::ShaderType::FRAGMENT> fragment(device, vsl_test::shader_path("triangle.frag.spv"));
        vsl::Shader<vsl::ShaderType::VERTEX> missing(device, vsl_test::shader_path("triangle.vert.spv"));
        vsl::pipeline_layout::ShaderGroup group("unchanged", {vertex, fragment});
        group.remove_shader("missing");
        group.remove_shader(missing);
        ASSERT_EQ(ShaderGroupAccess::Data(group)->shaders.size(), 2u);
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders[0], ShaderAccess::Owned(vertex));
        EXPECT_EQ(ShaderGroupAccess::Data(group)->shaders[1], ShaderAccess::Owned(fragment));
        auto layout = make_layout_state();
        group.injection(layout);
        ASSERT_EQ(layout.recipe().shaders.size(), 2u);
        EXPECT_EQ(layout.recipe().shaders[0]->shader_module, ShaderAccess::Owned(vertex)->shader_module);
        EXPECT_EQ(layout.recipe().shaders[1]->shader_module, ShaderAccess::Owned(fragment)->shader_module);
    }
} // namespace
