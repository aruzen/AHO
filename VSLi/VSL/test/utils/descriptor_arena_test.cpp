#include <VSL/lifetime/descriptor_access.hpp>
#include "support/vulkan_fixture.hpp"
#include "support/fixed_parent.hpp"
#include <VSL/lifetime/pipeline_recipe.hpp>
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/descriptor_bindings.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/sampler_access.hpp>
#include <VSL/lifetime/native_recording.hpp>
#include <VSL/utils/typed_compute.hpp>
#include <VSL/vulkan/commands/image_upload.hpp>
#include <VSL/vulkan/commands/image_readback.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
}

namespace {
    namespace t = vsl::typed;
    namespace l = vsl::lifetime;
    namespace s = vsl::storage;
    using enum s::ResourceKind;
    using Set = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                 t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using Schema = t::ResourceSchema<Set>;
    using TypedSet = t::TypedDescriptorSet<Set>;
    using Pipeline = t::TypedComputePipeline<Schema>;
    using UniformView = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>;
    using StorageView = t::TypedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::STORAGE_BUFFER>;
    constexpr std::array<t::ResourceRequest, 7> REQUESTS{{{{101}, DESCRIPTOR_SET}, {{102}, DESCRIPTOR_SET}, {{1}, BUFFER}, {{2}, BUFFER}, {{3}, BUFFER}, {{201}, DESCRIPTOR_LAYOUT}, {{202}, DESCRIPTOR_POOL}}};
    constexpr t::ResourcePlan<7> PLAN(REQUESTS, {t::LifetimeScope::FRAME, 2});
    const vsl::DescriptorLayoutCreateInfo LAYOUT_INFO{{Set::POINTS.begin(), Set::POINTS.end()}};
    const vsl::DescriptorPoolCreateInfo POOL_INFO{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 2},
                                                   {vsl::graphic_resource::Type::STORAGE_BUFFER, 2}},
                                                  2};
    std::array<l::ResourceInit, 7> inputs();
    void write_value(const vsl::BufferView& buffer, std::uint32_t value);
    std::uint32_t read_value(const vsl::BufferView& buffer);
    void make_host_visible(vsl::CommandManager manager);
    std::uint32_t execute_set(Pipeline& pipeline, vsl::CommandManager manager, const TypedSet& set, const vsl::BufferView& output);
    class DescriptorArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }

    protected:
        Pipeline make_pipeline();
    };

    std::array<l::ResourceInit, 7> inputs() {
        return {l::DescriptorSetInit{{202}, {201}}, l::DescriptorSetInit{{202}, {201}},
                vsl::BufferCreateInfo{4, vsl::MemoryType::UNIFORM_BUFFER},
                vsl::BufferCreateInfo{4, vsl::MemoryType::STORAGE_BUFFER},
                vsl::BufferCreateInfo{4, vsl::MemoryType::STORAGE_BUFFER}, LAYOUT_INFO, POOL_INFO};
    }
    void write_value(const vsl::BufferView& buffer, std::uint32_t value) {
        buffer.write(std::as_bytes(std::span(&value, 1)));
    }
    std::uint32_t read_value(const vsl::BufferView& buffer) {
        std::uint32_t value = 0;
        buffer.read_into(std::as_writable_bytes(std::span(&value, 1)));
        return value;
    }
    void make_host_visible(vsl::CommandManager manager) {
        const vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        const vsl::NativeRecordingContext recording(manager.get_buffer());
        vk::CommandBuffer(recording.native_command_buffer()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
    }
    Pipeline DescriptorArenaVulkanIntegrationTest::make_pipeline() {
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_frame.comp.spv"));
        return {device, vsl::pipeline_layout::ShaderGroup("arena descriptors", {shader})};
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, RecipeLayoutImportsAndHeapSetsKeepCompositeProvenanceAndRejectOnlyTheirParentCycle) {
        using Access = l::detail::OwnerAccess;
        std::weak_ptr<void> observed_arena, observed_context;
        std::optional<vsl::GraphicResource> kept;
        {
            constexpr std::array<t::ResourceRequest, 1> requests{{{{1}, DESCRIPTOR_LAYOUT}}};
            constexpr t::ResourcePlan<1> plan(requests);
            const vsl::DescriptorLayoutCreateInfo info{{{0, vsl::graphic_resource::Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE}}};
            const std::array<l::ResourceInit, 1> inputs{info};
            l::ResourceArena arena(device, plan, inputs);
            observed_arena = arena.owner();
            auto context = vsl_test::FixedParentOwner::Create(arena.owner_ref());
            observed_context = context;
            auto composite = arena.descriptor_layout({1});
            composite = DescriptorAccess::FromOwned(context->node.ref(context).alias(DescriptorAccess::Owned(composite).get()));
            l::PipelineRecipeDraft draft(device);
            draft.inject(vsl::pipeline_layout::ResourceBinding{{composite}});
            const auto snapshot = draft.snapshot();
            const auto& recipe = snapshot.definition();
            ASSERT_EQ(recipe.binding_layouts.size(), 1u);
            EXPECT_EQ(Access::Node(recipe.sources.at(recipe.binding_sources.front())), &context->node);
            const auto layouts = snapshot.layout_sources();
            ASSERT_EQ(layouts.size(), 1u);
            EXPECT_EQ(Access::Node(layouts.front().owner_ref()), &context->node);
            EXPECT_FALSE(DescriptorAccess::Owned(layouts.front()).owner_before(context));
            EXPECT_FALSE(context.owner_before(DescriptorAccess::Owned(layouts.front()).share()));
            constexpr std::array<t::ResourceRequest, 1> imports{{{{2}, DESCRIPTOR_LAYOUT, true}}};
            constexpr t::ResourcePlan<1> import_plan(imports);
            const std::array<l::ResourceInit, 1> import_inputs{layouts.front()};
            l::ResourceArena external(device, import_plan, import_inputs);
            EXPECT_EQ(Access::Node(external.descriptor_layout({2}).owner_ref()), &context->node);
            const auto parent = context->node.ref(context);
            vsl_test::expect_fixed_parent(external.owner_ref(), parent);
            vsl_test::expect_fixed_parent(external.owner_ref(), arena.owner_ref(), false);
            vsl::GraphicResourceManager manager(device);
            auto pool = manager.make({{vsl::graphic_resource::Type::STORAGE_BUFFER, 1}});
            auto [success, set] = pool.bind(external.descriptor_layout({2}));
            ASSERT_TRUE(success);
            ASSERT_TRUE(DescriptorAccess::Owned(set)->descriptor_set);
            vsl_test::expect_fixed_parent(set.owner_ref(), parent);
            vsl_test::expect_fixed_parent(set.owner_ref(), arena.owner_ref(), false);
            kept = std::move(set);
        }
        EXPECT_FALSE(observed_context.expired());
        EXPECT_FALSE(observed_arena.expired());
        ASSERT_TRUE(kept);
        EXPECT_EQ(kept->get_pool().get_manager(), nullptr);
        kept.reset();
        EXPECT_TRUE(observed_context.expired());
        EXPECT_TRUE(observed_arena.expired());
    }
    std::uint32_t execute_set(Pipeline& pipeline, vsl::CommandManager manager, const TypedSet& set, const vsl::BufferView& output) {
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << pipeline.bind_resources(set).dispatch(1);
        make_host_visible(manager);
        if (phase.submit() != vsl::ComputePhase::SubmitResult::SUCCESS || !phase.completion().wait())
            throw std::runtime_error("Descriptor ownership test submission failed");
        return read_value(output);
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, FreezeRejectsASeparateControlBlockBeforePollingTheRealResource) {
        vsl::GraphicResourceManager descriptors(device);
        TypedSet source(device, descriptors);
        auto forged = DescriptorAccess::FromShared(std::shared_ptr<vsl::impl::GraphicResourceImpl>(std::make_shared<int>(0), DescriptorAccess::Owned(source.backend()).get()));
        auto imported = TypedSet::FromResource(device, forged);
        EXPECT_THROW(imported.freeze_dependencies(), std::invalid_argument);
        EXPECT_EQ(DescriptorAccess::Owned(source.backend())->owner_context->policy, l::DependencyPolicy::DYNAMIC);
        EXPECT_FALSE(DescriptorAccess::Owned(source.backend())->owner_context->node.updating());
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, FreezeFailureKeepsDynamicOwnersAndCanBeRetriedAfterCancellation) {
        const auto initializers = inputs();
        l::ResourceArena arena(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto first = TypedSet::FromResource(device, arena.descriptor_set({101}));
        auto second = TypedSet::FromResource(device, arena.descriptor_set({102}));
        t::TypedUniformBuffer<std::uint32_t> external(device, 7u), other(device, 9u);
        for (auto* set : {&first, &second}) {
            set->write<0>(external);
            set->write<1>(StorageView::FromView(arena.buffer({2}), 1));
        }
        EXPECT_THROW(first.freeze_dependencies(), std::invalid_argument);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        second.resource_use()->record(manager.get_buffer());
        EXPECT_THROW(arena.freeze_dependencies(), std::logic_error);
        EXPECT_EQ(DescriptorAccess::Owned(first.backend())->owner_context->policy, l::DependencyPolicy::DYNAMIC);
        EXPECT_FALSE(DescriptorAccess::Owned(first.backend())->owner_context->node.updating());
        phase.cancel();
        EXPECT_NO_THROW(arena.freeze_dependencies());
        EXPECT_EQ(DescriptorAccess::Owned(first.backend())->owner_context->policy, l::DependencyPolicy::FIXED);
        EXPECT_TRUE(DescriptorAccess::Owned(first.backend())->bindings.at(0).front().owners.empty());
        EXPECT_TRUE(DescriptorAccess::Owned(second.backend())->bindings.at(0).front().owners.empty());
        EXPECT_NO_THROW(first.write<0>(external));
        EXPECT_THROW(second.write<0>(other), std::invalid_argument);
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, FreezeGatesPollCallbacksAndPinsTheRootWhenItsFacadeIsDestroyed) {
        const auto initializers = inputs();
        std::optional<l::ResourceArena> arena(std::in_place, device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        std::optional<TypedSet> first(TypedSet::FromResource(device, arena->descriptor_set({101})));
        std::optional<TypedSet> second(TypedSet::FromResource(device, arena->descriptor_set({102})));
        first->write<0>(UniformView::FromView(arena->buffer({1}), 1));
        second->write<0>(UniformView::FromView(arena->buffer({1}), 1));
        first->write<1>(StorageView::FromView(arena->buffer({2}), 1));
        second->write<1>(StorageView::FromView(arena->buffer({3}), 1));
        std::weak_ptr<void> root = arena->owner();
        auto manager = make_manager();
        auto reentrant = make_manager().independent(1);
        auto recording = reentrant.start_phase<vsl::ComputePhase>();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        second->resource_use()->record(manager.get_buffer());
        bool callback = false;
        manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(0), [&](int* value) {
            delete value;
            callback = true;
            EXPECT_THROW(first->resource_use()->record(reentrant.get_buffer()), std::logic_error);
            first.reset();
            second.reset();
            arena.reset();
            EXPECT_FALSE(root.expired());
        }));
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        device.data_->device.waitIdle();
        EXPECT_NO_THROW(arena->freeze_dependencies());
        EXPECT_TRUE(callback);
        EXPECT_FALSE(arena);
        recording.cancel();
        EXPECT_TRUE(root.expired());
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, ReorderedDependenciesAndTwoSetSlotsUseTypedCommandsAndOneOwner) {
        const auto initializers = inputs();
        l::ResourceArena arena(device, PLAN, initializers);
        auto first = TypedSet::FromResource(device, arena.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        auto second = TypedSet::FromResource(device, arena.descriptor_set({102}), vsl::ResourceRetention::COMPLETION);
        const auto root = arena.owner();
        const auto base = reinterpret_cast<std::uintptr_t>(DescriptorAccess::Owned(first.backend()).get()) - PLAN.placement({101}).offset;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(DescriptorAccess::Owned(second.backend()).get()) - base, PLAN.placement({102}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(DescriptorAccess::Owned(arena.descriptor_pool({202})).get()) - base, PLAN.placement({202}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(DescriptorAccess::Owned(arena.descriptor_layout({201})).get()) - base, PLAN.placement({201}).offset);
        EXPECT_NE(&DescriptorAccess::Owned(first.backend())->use, &DescriptorAccess::Owned(second.backend())->use);
        EXPECT_FALSE(DescriptorAccess::Owned(first.backend()).owner_before(DescriptorAccess::Owned(second.backend())));
        EXPECT_FALSE(DescriptorAccess::Owned(second.backend()).owner_before(DescriptorAccess::Owned(first.backend())));
        const auto uniform = UniformView::FromView(arena.buffer({1}), 1);
        first.write<0>(uniform);
        first.write<1>(StorageView::FromView(arena.buffer({2}), 1));
        second.write<0>(uniform);
        second.write<1>(StorageView::FromView(arena.buffer({3}), 1));
        write_value(arena.buffer({1}), 37);
        auto pipeline = make_pipeline();
        auto manager = make_manager().independent(1);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << pipeline.bind_resources(first).dispatch(1) << pipeline.bind_resources(second).dispatch(1);
        const auto& retained = phase.completion().data_->work.lock()->completion_resources;
        EXPECT_EQ(std::ranges::count_if(retained, [&](const auto& owner) { return !root.owner_before(owner) && !owner.owner_before(root); }), 1);
        make_host_visible(manager);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(read_value(arena.buffer({2})), 37u);
        EXPECT_EQ(read_value(arena.buffer({3})), 37u);
        const auto pool = arena.descriptor_pool({202});
        const auto from_set = arena.descriptor_set({101}).get_pool();
        EXPECT_FALSE(DescriptorAccess::Owned(pool).owner_before(DescriptorAccess::Owned(from_set)));
        EXPECT_FALSE(DescriptorAccess::Owned(from_set).owner_before(DescriptorAccess::Owned(pool)));
        EXPECT_EQ(DescriptorAccess::Owned(pool)->resources.size(), 2u);
        using Wrong = t::DescriptorSet<0, t::UniformBinding<9, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
        EXPECT_THROW(t::TypedDescriptorSet<Wrong>::FromResource(device, arena.descriptor_set({101})), std::invalid_argument);
        EXPECT_THROW(TypedSet::FromResource({}, arena.descriptor_set({101})), std::invalid_argument);
        const auto owned = arena.descriptor_set({101});
        const auto group = vsl::ResourceUseGroup::ForDescriptor(Set::BINDING_COUNT, owned);
        EXPECT_TRUE(group->tracks_descriptor(owned));
        EXPECT_TRUE(group->tracks_descriptor(arena.descriptor_set({101})));
        EXPECT_FALSE(group->tracks_descriptor(arena.descriptor_set({102})));
        const auto borrowed = DescriptorAccess::FromShared(std::shared_ptr<vsl::impl::GraphicResourceImpl>(std::shared_ptr<void>{}, DescriptorAccess::Owned(owned).get()));
        EXPECT_THROW(TypedSet::FromResource(device, borrowed), std::invalid_argument);
        EXPECT_THROW(vsl::ResourceUseGroup::ForDescriptor(2, borrowed), std::invalid_argument);
        const vsl::LogicalDeviceAccessor borrowed_device{std::shared_ptr<vsl::impl::LogicalDeviceImpl>(std::shared_ptr<void>{}, device.data_.get())};
        EXPECT_THROW(TypedSet::FromResource(borrowed_device, owned), std::invalid_argument);
        EXPECT_THROW(l::ResourceArena(borrowed_device, PLAN, initializers), std::invalid_argument);
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, SameRootPhysicalAndLogicalAliasesAreBorrowedUntilCompletionOrCancellation) {
        auto pipeline = make_pipeline();
        for (const bool cancel : {false, true}) {
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            std::weak_ptr<void> observed;
            {
                const auto initializers = inputs();
                l::ResourceArena arena(device, PLAN, initializers);
                observed = arena.owner();
                write_value(arena.buffer({1}), 41);
                auto set = TypedSet::FromResource(device, arena.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
                set.write<0>(UniformView::FromView(arena.buffer({1}), 1, 0, arena.buffer({3}).resource_use()));
                set.write<1>(StorageView::FromView(arena.buffer({2}), 1));
                const auto& element = DescriptorAccess::Owned(set.backend())->bindings.at(0).front();
                EXPECT_TRUE(element.owners.empty());
                EXPECT_TRUE(element.constraint.owners.empty());
                EXPECT_EQ(element.constraint.states.size(), 1u);
                phase << pipeline.bind_resources(set).dispatch(1);
                EXPECT_TRUE(arena.borrow_buffer({3}).use().busy());
            }
            EXPECT_FALSE(observed.expired());
            if (cancel)
                phase.cancel();
            else {
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(phase.completion().wait());
            }
            EXPECT_TRUE(observed.expired());
        }
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, IndependentLogicalDependenciesAndUnknownOwnerRejectionPreserveGPUData) {
        std::optional<l::ResourceArena> arena;
        std::optional<TypedSet> set;
        std::weak_ptr<vsl::ResourceUseState> logical;
        {
            const std::array dependencies{l::LogicalUseState{}};
            logical = dependencies.front().resource_use().state.share();
            const auto initializers = inputs();
            arena.emplace(device, PLAN, initializers, s::ArenaMemory(PLAN.bytes(), PLAN.alignment()), dependencies);
            set.emplace(TypedSet::FromResource(device, arena->descriptor_set({101}), vsl::ResourceRetention::COMPLETION));
            set->write<0>(UniformView::FromView(arena->buffer({1}), 1, 0, dependencies.front().resource_use()));
            set->write<1>(StorageView::FromView(arena->buffer({2}), 1));
            write_value(arena->buffer({1}), 53);
        }
        EXPECT_FALSE(logical.expired());
        auto unknown = std::make_shared<vsl::ResourceUseState>();
        t::TypedUniformBuffer<std::uint32_t> external(device, std::uint32_t{99});
        EXPECT_THROW(set->write<0>(external), std::invalid_argument);
        auto native = set->backend();
        EXPECT_THROW(native.update(external.backend()), std::invalid_argument);
        EXPECT_THROW(set->write<0>(UniformView::FromView(arena->buffer({1}), 1, 0, {unknown, {}})), std::invalid_argument);
        EXPECT_THROW(set->resource_use()->replace(0, {{unknown, {}}}), std::invalid_argument);
        EXPECT_NO_THROW(set->require_complete());
        auto manager = make_manager().independent(1);
        auto pipeline = make_pipeline();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << pipeline.bind_resources(*set).dispatch(1);
        EXPECT_TRUE(logical.lock()->busy());
        EXPECT_FALSE(unknown->busy());
        make_host_visible(manager);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(read_value(arena->buffer({2})), 53u);
        native = {};
        set.reset();
        arena.reset();
        EXPECT_TRUE(logical.expired());
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, DeclaredExternalStorageSurvivesWrapperDestructionAndBindingReplacement) {
        constexpr std::array<t::ResourceRequest, 8> requests{{{{101}, DESCRIPTOR_SET}, {{102}, DESCRIPTOR_SET}, {{1}, BUFFER}, {{2}, BUFFER}, {{3}, BUFFER}, {{201}, DESCRIPTOR_LAYOUT}, {{202}, DESCRIPTOR_POOL}, {{9}, BUFFER, true}}};
        const t::ResourcePlan<8> plan(requests);
        std::optional<l::ResourceArena> arena;
        std::optional<TypedSet> set;
        std::weak_ptr<void> external_owner;
        {
            t::TypedUniformBuffer<std::uint32_t> external(device, std::uint32_t{61});
            external_owner = external.storage_view().owner();
            const auto base = inputs();
            const std::array<l::ResourceInit, 8> initializers{base[0], base[1], base[2], base[3], base[4], base[5], base[6],
                                                              external.storage_view()};
            arena.emplace(device, plan, initializers);
            set.emplace(TypedSet::FromResource(device, arena->descriptor_set({101}), vsl::ResourceRetention::COMPLETION));
            set->write<0>(UniformView::FromView(arena->buffer({9}), 1));
            set->write<1>(StorageView::FromView(arena->buffer({2}), 1));
        }
        EXPECT_FALSE(external_owner.expired());
        auto manager = make_manager().independent(1);
        auto pipeline = make_pipeline();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << pipeline.bind_resources(*set).dispatch(1);
            make_host_visible(manager);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_EQ(read_value(arena->buffer({2})), 61u);
        set->write<0>(UniformView::FromView(arena->buffer({1}), 1));
        EXPECT_FALSE(external_owner.expired());
        set.reset();
        arena.reset();
        EXPECT_TRUE(external_owner.expired());
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, ImportedPoolTracksEverySetAndInvalidatesMetadataOnlyAfterAllUsesFinish) {
        vsl::GraphicResourceManager manager(device);
        vsl::graphic_resource::BindingLayout layout(device, LAYOUT_INFO.bindings);
        auto pool = manager.make(POOL_INFO.descriptors);
        constexpr std::array<t::ResourceRequest, 6> requests{{{{101}, DESCRIPTOR_SET}, {{102}, DESCRIPTOR_SET}, {{201}, DESCRIPTOR_LAYOUT, true}, {{202}, DESCRIPTOR_POOL, true}, {{1}, BUFFER}, {{2}, BUFFER}}};
        const t::ResourcePlan<6> plan(requests);
        const std::array<l::ResourceInit, 6> initializers{l::DescriptorSetInit{{202}, {201}}, l::DescriptorSetInit{{202}, {201}}, layout, pool,
                                                          vsl::BufferCreateInfo{4, vsl::MemoryType::UNIFORM_BUFFER},
                                                          vsl::BufferCreateInfo{4, vsl::MemoryType::STORAGE_BUFFER}};
        l::ResourceArena arena(device, plan, initializers);
        auto first = TypedSet::FromResource(device, arena.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        auto second = TypedSet::FromResource(device, arena.descriptor_set({102}), vsl::ResourceRetention::COMPLETION);
        for (auto* set : {&first, &second}) {
            set->write<0>(UniformView::FromView(arena.buffer({1}), 1));
            set->write<1>(StorageView::FromView(arena.buffer({2}), 1));
        }
        EXPECT_EQ(DescriptorAccess::Owned(pool)->resources.size(), 2u);
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 2u);
        const auto imported_pool = arena.descriptor_pool({202});
        EXPECT_FALSE(DescriptorAccess::Owned(imported_pool).owner_before(DescriptorAccess::Owned(pool)));
        EXPECT_FALSE(DescriptorAccess::Owned(pool).owner_before(DescriptorAccess::Owned(imported_pool)));
        auto commands = make_manager().independent(1), other = commands.independent(1);
        auto pending = commands.start_phase<vsl::ComputePhase>();
        auto submitted = other.start_phase<vsl::ComputePhase>();
        first.resource_use()->record(commands.get_buffer());
        second.resource_use()->record(other.get_buffer());
        ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(submitted.completion().wait());
        EXPECT_THROW(manager.destroy(pool), std::logic_error);
        EXPECT_NO_THROW(first.require_complete());
        pending.cancel();
        EXPECT_NO_THROW(manager.destroy(pool));
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 0u);
        EXPECT_FALSE(DescriptorAccess::Owned(first.backend())->descriptor_set);
        EXPECT_FALSE(DescriptorAccess::Owned(second.backend())->descriptor_set);
        EXPECT_TRUE(DescriptorAccess::Owned(first.backend())->bindings.empty());
        EXPECT_TRUE(DescriptorAccess::Owned(second.backend())->bindings.empty());
        EXPECT_THROW(first.require_complete(), std::logic_error);
        EXPECT_THROW(second.resource_use()->record(other.get_buffer()), std::logic_error);
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, LaterSetCapacityFailureRollsBackNativeAllocationAndRegionBeforeRetry) {
        auto info = POOL_INFO;
        auto pool = DescriptorAccess::FromShared(l::make_heap_descriptor_pool(device, info));
        vsl::graphic_resource::BindingLayout layout(device, LAYOUT_INFO.bindings);
        auto [allocated, occupied] = pool.bind(layout);
        ASSERT_TRUE(allocated);
        ASSERT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 1u);
        constexpr std::array<t::ResourceRequest, 4> requests{{{{101}, DESCRIPTOR_SET}, {{102}, DESCRIPTOR_SET}, {{201}, DESCRIPTOR_LAYOUT, true}, {{202}, DESCRIPTOR_POOL, true}}};
        const t::ResourcePlan<4> plan(requests);
        const std::array<l::ResourceInit, 4> initializers{l::DescriptorSetInit{{202}, {201}}, l::DescriptorSetInit{{202}, {201}}, layout, pool};
        s::ArenaMemory memory(plan.bytes(), plan.alignment());
        EXPECT_THROW((l::ResourceArena(device, plan, initializers, memory)), std::runtime_error);
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 1u);
        EXPECT_EQ(DescriptorAccess::Owned(pool)->resources.size(), 1u);
        EXPECT_TRUE(DescriptorAccess::Owned(occupied)->descriptor_set);
        occupied = {};
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 0u);
        {
            l::ResourceArena retried(device, plan, initializers, memory);
            EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 2u);
            EXPECT_EQ(DescriptorAccess::Owned(pool)->resources.size(), 2u);
            EXPECT_TRUE(DescriptorAccess::Owned(retried.descriptor_set({101}))->descriptor_set);
            EXPECT_TRUE(DescriptorAccess::Owned(retried.descriptor_set({102}))->descriptor_set);
        }
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 0u);
        auto wrong = initializers;
        wrong[1] = l::DescriptorSetInit{{201}, {202}};
        EXPECT_THROW((l::ResourceArena(device, plan, wrong, memory)), std::invalid_argument);
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 0u);
        EXPECT_NO_THROW((l::ResourceArena(device, plan, initializers, memory)));
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, ImportedPoolGetterDoesNotRetainTheSetArenaAndRemainsReusable) {
        vsl::GraphicResourceManager manager(device);
        vsl::graphic_resource::BindingLayout layout(device, LAYOUT_INFO.bindings);
        auto pool = manager.make(POOL_INFO.descriptors);
        constexpr std::array<t::ResourceRequest, 3> requests{{{{101}, DESCRIPTOR_SET}, {{201}, DESCRIPTOR_LAYOUT, true}, {{202}, DESCRIPTOR_POOL, true}}};
        const t::ResourcePlan<3> plan(requests);
        const std::array<l::ResourceInit, 3> initializers{l::DescriptorSetInit{{202}, {201}}, layout, pool};
        vsl::graphic_resource::Pool retained;
        std::weak_ptr<void> observed;
        {
            l::ResourceArena arena(device, plan, initializers);
            observed = arena.owner();
            auto set = arena.descriptor_set({101});
            retained = set.get_pool();
            EXPECT_FALSE(DescriptorAccess::Owned(retained).owner_before(DescriptorAccess::Owned(pool)));
            EXPECT_FALSE(DescriptorAccess::Owned(pool).owner_before(DescriptorAccess::Owned(retained)));
        }
        EXPECT_TRUE(observed.expired());
        EXPECT_EQ(DescriptorAccess::Owned(pool)->allocated_sets, 0u);
        auto [success, resource] = retained.bind(layout);
        EXPECT_TRUE(success);
        EXPECT_TRUE(DescriptorAccess::Owned(resource)->descriptor_set);
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, SameRootImageAndSamplerBindingsDoNotKeepTheArenaAlive) {
        using Textures = t::DescriptorSet<0, t::TextureBinding<2, vsl::ShaderFlag::COMPUTE>>;
        constexpr std::array<t::ResourceRequest, 5> requests{{{{1}, DESCRIPTOR_SET}, {{2}, IMAGE}, {{3}, SAMPLER}, {{4}, DESCRIPTOR_LAYOUT}, {{5}, DESCRIPTOR_POOL}}};
        const t::ResourcePlan<5> plan(requests);
        const vsl::DescriptorLayoutCreateInfo layout{{Textures::POINTS.begin(), Textures::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, 1}}, 1};
        const std::array<l::ResourceInit, 5> initializers{l::DescriptorSetInit{{5}, {4}},
                                                          vsl::ImageCreateInfo{vsl::ImageType::SAMPLED, 1, 1},
                                                          vsl::SamplerCreateInfo{}, layout, pool};
        auto manager = make_manager().independent(1);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        std::weak_ptr<void> observed;
        {
            l::ResourceArena arena(device, plan, initializers);
            observed = arena.owner();
            auto set = t::TypedDescriptorSet<Textures>::FromResource(device, arena.descriptor_set({1}), vsl::ResourceRetention::COMPLETION);
            set.write<2>(t::TypedTexture(arena.image({2}), arena.sampler({3})));
            EXPECT_TRUE(DescriptorAccess::Owned(set.backend())->bindings.at(2).front().owners.empty());
            set.resource_use()->record(manager.get_buffer());
            EXPECT_TRUE(arena.image({2}).use_state().busy());
            EXPECT_TRUE(arena.sampler({3}).use_state().busy());
        }
        EXPECT_FALSE(observed.expired());
        phase.cancel();
        EXPECT_TRUE(observed.expired());
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, ReorderedNativeBindingsImportAndKeepLegacyConstraintIndicesInNativeOrder) {
        auto initializers = inputs();
        auto layout = LAYOUT_INFO;
        std::ranges::reverse(layout.bindings);
        initializers[5] = layout;
        const std::array dependencies{l::LogicalUseState{}, l::LogicalUseState{}};
        l::ResourceArena arena(device, PLAN, initializers, s::ArenaMemory(PLAN.bytes(), PLAN.alignment()), dependencies);
        auto set = TypedSet::FromResource(device, arena.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        set.write<0>(UniformView::FromView(arena.buffer({1}), 1, 0, dependencies[0].resource_use()));
        set.write<1>(StorageView::FromView(arena.buffer({2}), 1, 0, dependencies[0].resource_use()));
        set.resource_use()->replace(0, {dependencies[1].resource_use()});
        EXPECT_EQ(DescriptorAccess::Owned(set.backend())->bindings.at(0).front().constraint.states.front(), dependencies[0].resource_use().state.get());
        EXPECT_EQ(DescriptorAccess::Owned(set.backend())->bindings.at(1).front().constraint.states.front(), dependencies[1].resource_use().state.get());
        write_value(arena.buffer({1}), 73);
        auto pipeline = make_pipeline();
        auto manager = make_manager().independent(1);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << pipeline.bind_resources(set).dispatch(1);
        EXPECT_TRUE(dependencies[0].busy());
        EXPECT_TRUE(dependencies[1].busy());
        make_host_visible(manager);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_EQ(read_value(arena.buffer({2})), 73u);
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, CombinedAndSeparateArenaDescriptorsSampleUploadedPixelsAndReadBack) {
        using SampleSet = t::DescriptorSet<0, t::StorageBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                           t::TextureBinding<1, vsl::ShaderFlag::COMPUTE>,
                                           t::DescriptorBinding<2, vsl::graphic_resource::Type::SAMPLED_IMAGE, vsl::ShaderFlag::COMPUTE>,
                                           t::DescriptorBinding<3, vsl::graphic_resource::Type::SAMPLER, vsl::ShaderFlag::COMPUTE>>;
        constexpr std::array<t::ResourceRequest, 6> requests{{{{1}, DESCRIPTOR_SET}, {{2}, BUFFER}, {{3}, IMAGE}, {{4}, SAMPLER}, {{5}, DESCRIPTOR_LAYOUT}, {{6}, DESCRIPTOR_POOL}}};
        const t::ResourcePlan<6> plan(requests);
        const vsl::DescriptorLayoutCreateInfo layout{{SampleSet::POINTS.begin(), SampleSet::POINTS.end()}};
        const vsl::DescriptorPoolCreateInfo pool{{{vsl::graphic_resource::Type::STORAGE_BUFFER, 1},
                                                  {vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, 1},
                                                  {vsl::graphic_resource::Type::SAMPLED_IMAGE, 1},
                                                  {vsl::graphic_resource::Type::SAMPLER, 1}},
                                                 1};
        const std::array<l::ResourceInit, 6> initializers{l::DescriptorSetInit{{6}, {5}}, vsl::BufferCreateInfo{8, vsl::MemoryType::STORAGE_BUFFER},
                                                          vsl::ImageCreateInfo{vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST | vsl::ImageType::TRANSFER_SRC,
                                                                               1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA},
                                                          vsl::SamplerCreateInfo{vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE}, layout, pool};
        std::weak_ptr<void> observed;
        const std::array<std::uint8_t, 4> color{37, 83, 0, 255};
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_arena_images.comp.spv"));
        t::TypedComputePipeline<t::ResourceSchema<SampleSet>> pipeline(device, vsl::pipeline_layout::ShaderGroup("placed sampling", {shader}));
        {
            auto manager = make_manager().independent(1);
            l::ResourceArena arena(device, plan, initializers);
            observed = arena.owner();
            auto set = t::TypedDescriptorSet<SampleSet>::FromResource(device, arena.descriptor_set({1}), vsl::ResourceRetention::COMPLETION);
            set.write<0>(StorageView::FromView(arena.buffer({2}), 2));
            set.write<1>(t::TypedTexture(arena.image({3}), arena.sampler({4})));
            set.write<2>(t::TypedSampledImage(arena.image({3})));
            set.write<3>(t::TypedSampler(arena.sampler({4})));
            vsl::StagingBuffer<> pixels(device, color);
            vsl::StagingBuffer<vsl::MemoryType::TRANSFER_DESTINATION> readback(device, color.size());
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << vsl::command::ImageUpload(&pixels, arena.image({3}), vsl::ImageLayout::UNDEFINED)
                  << pipeline.bind_resources(set).dispatch(1)
                  << vsl::command::ImageReadback(&readback, arena.image({3}), vsl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
            make_host_visible(manager);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            std::array<std::uint32_t, 2> values{};
            arena.buffer({2}).read_into(std::as_writable_bytes(std::span(values)));
            EXPECT_EQ(values, (std::array<std::uint32_t, 2>{37, 83}));
            std::array<std::uint8_t, 4> actual{};
            readback.read_into(std::as_writable_bytes(std::span(actual)));
            EXPECT_EQ(actual, color);
        }
        EXPECT_TRUE(observed.expired());
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, DynamicExternalStorageIsReleasedOnlyAfterItsLastBindingIsReplaced) {
        const auto initializers = inputs();
        l::ResourceArena arena(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto first = TypedSet::FromResource(device, arena.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        auto second = TypedSet::FromResource(device, arena.descriptor_set({102}), vsl::ResourceRetention::COMPLETION);
        std::weak_ptr<void> external;
        write_value(arena.buffer({1}), 17);
        {
            l::ResourceArena other(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
            external = other.owner();
            write_value(other.buffer({1}), 73);
            first.write<0>(UniformView::FromView(other.buffer({1}), 1));
            second.write<0>(UniformView::FromView(other.buffer({1}), 1));
        }
        first.write<1>(StorageView::FromView(arena.buffer({2}), 1));
        second.write<1>(StorageView::FromView(arena.buffer({3}), 1));
        EXPECT_FALSE(external.expired());
        auto pipeline = make_pipeline();
        auto manager = make_manager().independent(1);
        EXPECT_EQ(execute_set(pipeline, manager, first, arena.buffer({2})), 73u);
        first.write<0>(UniformView::FromView(arena.buffer({1}), 1));
        EXPECT_FALSE(external.expired());
        EXPECT_EQ(execute_set(pipeline, manager, first, arena.buffer({2})), 17u);
        EXPECT_EQ(execute_set(pipeline, manager, second, arena.buffer({3})), 73u);
        second.write<0>(UniformView::FromView(arena.buffer({1}), 1));
        EXPECT_TRUE(external.expired());
        EXPECT_EQ(execute_set(pipeline, manager, second, arena.buffer({3})), 17u);
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, NativeViewUpdatePreservesTheManagedCompositeOwnerWhenRejectingACycle) {
        struct Context {
            l::DependencyNode node;
            vsl::BufferView storage;
            explicit Context(vsl::BufferView storage) : storage(std::move(storage)) {}
            ~Context() { node.retire(); }
            static void Visit(const void* context, l::DependencyNode::EdgeVisitor visitor, void* output) {
                visitor(output, static_cast<const Context*>(context)->storage.owner_ref());
            }
        };
        const auto initializers = inputs();
        l::ResourceArena arena(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto context = std::make_shared<Context>(arena.buffer({1}));
        context->node.publish(context, context.get(), Context::Visit);
        const auto composite = l::detail::BufferAccess::View(context->storage.ref(), context->node.ref(context), 0, 4);
        l::ResourceArena external(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto external_set = external.descriptor_set({101});
        ASSERT_NO_THROW(external_set.update(composite, 0));
        const auto& owners = DescriptorAccess::Owned(external_set)->bindings.at(0).front().owners;
        ASSERT_EQ(owners.size(), 1u);
        EXPECT_TRUE(owners.front().managed());
        EXPECT_FALSE(owners.front().owner_before(context));
        EXPECT_FALSE(context.owner_before(owners.front().share()));
        auto set = arena.descriptor_set({101});
        EXPECT_THROW(set.update(composite, 0), std::invalid_argument);
        EXPECT_TRUE(DescriptorAccess::Owned(set)->bindings.empty());
        EXPECT_NO_THROW(set.update(arena.buffer({1}), 0));
        EXPECT_EQ(DescriptorAccess::Owned(set)->bindings.at(0).size(), 1u);
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, ImageAndSamplerRoundTripsKeepCompositeProvenanceAndRejectOnlyTheOwningArenaCycle) {
        struct Context {
            l::DependencyNode node;
            l::OwnerRef physical;
            explicit Context(l::OwnerRef physical) : physical(std::move(physical)) {}
            ~Context() { node.retire(); }
            static void Visit(const void* context, l::DependencyNode::EdgeVisitor visitor, void* output) {
                visitor(output, static_cast<const Context*>(context)->physical);
            }
        };
        std::weak_ptr<Context> observed;
        {
            constexpr std::array<t::ResourceRequest, 5> requests{{{{1}, IMAGE}, {{2}, SAMPLER}, {{3}, DESCRIPTOR_LAYOUT}, {{4}, DESCRIPTOR_POOL}, {{5}, DESCRIPTOR_SET}}};
            constexpr t::ResourcePlan<5> plan(requests);
            const std::array<l::ResourceInit, 5> initializers{
                vsl::ImageCreateInfo{vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST, 1, 1},
                vsl::SamplerCreateInfo{},
                vsl::DescriptorLayoutCreateInfo{{{0, vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, vsl::ShaderFlag::FRAGMENT}}},
                vsl::DescriptorPoolCreateInfo{{{vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, 1}}, 1},
                l::DescriptorSetInit{{4}, {3}}};
            l::ResourceArena first(device, plan, initializers, l::DependencyPolicy::DYNAMIC);
            l::ResourceArena second(device, plan, initializers, l::DependencyPolicy::DYNAMIC);
            auto context = std::make_shared<Context>(first.owner_ref());
            observed = context;
            context->node.publish(context, context.get(), Context::Visit);
            const auto physical_image = first.image({1});
            const auto physical_sampler = first.sampler({2});
            const auto image = l::detail::ImageAccess::FromOwned(context->node.ref(context).alias(l::detail::ImageAccess::Owned(physical_image).get()));
            const auto sampler = l::detail::SamplerAccess::FromOwned(context->node.ref(context).alias(l::detail::SamplerAccess::Owned(physical_sampler).get()));
            const auto value = t::detail::DescriptorValue::FromTexture(image, sampler);
            const auto check = [&](const auto& resource) {
                const auto owner = resource.resource_use().owner;
                EXPECT_TRUE(owner.managed());
                EXPECT_FALSE(owner.owner_before(context));
                EXPECT_FALSE(context.owner_before(owner.share()));
            };
            check(value.image());
            check(value.sampler());
            check(t::detail::DescriptorValue::FromImage(image).image());
            check(t::detail::DescriptorValue::FromSampler(sampler).sampler());
            EXPECT_EQ(&value.image().use_state(), &physical_image.use_state());
            EXPECT_EQ(&value.sampler().use_state(), &physical_sampler.use_state());
            constexpr std::array<t::ResourceRequest, 2> external_requests{{{{10}, IMAGE, true}, {{20}, SAMPLER, true}}};
            constexpr t::ResourcePlan<2> external_plan(external_requests);
            const std::array<l::ResourceInit, 2> external_inputs{value.image(), value.sampler()};
            l::ResourceArena external(device, external_plan, external_inputs);
            check(external.image({10}));
            check(external.sampler({20}));
            auto accepted = second.descriptor_set({5});
            ASSERT_NO_THROW(accepted.update(external.image({10}), external.sampler({20}), 0));
            const auto& owners = DescriptorAccess::Owned(accepted)->bindings.at(0).front().owners;
            ASSERT_FALSE(owners.empty());
            for (const auto& owner : owners) {
                EXPECT_TRUE(owner.managed());
                EXPECT_FALSE(owner.owner_before(context));
                EXPECT_FALSE(context.owner_before(owner.share()));
            }
            auto rejected = first.descriptor_set({5});
            EXPECT_THROW(rejected.update(external.image({10}), external.sampler({20}), 0), std::invalid_argument);
            EXPECT_TRUE(DescriptorAccess::Owned(rejected)->bindings.empty());
            EXPECT_NO_THROW(rejected.update(physical_image, physical_sampler, 0));
        }
        EXPECT_TRUE(observed.expired());
    }

    TEST_F(DescriptorArenaVulkanIntegrationTest, DynamicManagedCyclesRejectPhysicalAndLogicalUpdatesInBothDirections) {
        const auto initializers = inputs();
        l::ResourceArena a(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        l::ResourceArena b(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto first = TypedSet::FromResource(device, a.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        auto second = TypedSet::FromResource(device, b.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        write_value(a.buffer({1}), 37);
        write_value(b.buffer({1}), 83);
        first.write<0>(UniformView::FromView(b.buffer({1}), 1));
        first.write<1>(StorageView::FromView(a.buffer({2}), 1));
        second.write<0>(UniformView::FromView(b.buffer({1}), 1));
        second.write<1>(StorageView::FromView(b.buffer({2}), 1));
        EXPECT_THROW(second.write<0>(UniformView::FromView(a.buffer({1}), 1)), std::invalid_argument);
        const auto logical = std::make_shared<vsl::ResourceUseState>();
        EXPECT_THROW(second.write<0>(UniformView::FromView(b.buffer({1}), 1, 0, {logical, a.owner_ref()})), std::invalid_argument);
        EXPECT_THROW(second.resource_use()->replace(0, {{logical, first.backend().owner_ref()}}), std::invalid_argument);
        EXPECT_TRUE(DescriptorAccess::Owned(second.backend())->bindings.at(0).front().constraint.states.empty());
        auto pipeline = make_pipeline();
        auto manager = make_manager().independent(1);
        EXPECT_EQ(execute_set(pipeline, manager, second, b.buffer({2})), 83u);
        first.write<0>(UniformView::FromView(a.buffer({1}), 1));
        EXPECT_NO_THROW(second.write<0>(UniformView::FromView(a.buffer({1}), 1)));
        EXPECT_THROW(first.write<0>(UniformView::FromView(b.buffer({1}), 1)), std::invalid_argument);
        EXPECT_THROW(first.write<1>(StorageView::FromView(b.buffer({2}), 1)), std::invalid_argument);
        EXPECT_EQ(execute_set(pipeline, manager, second, b.buffer({2})), 37u);
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, HeapSetPoolAndLayoutParentsPreventAnArenaCycleAndKeepTheNativeStorageAlive) {
        const std::array<t::ResourceRequest, 6> requests{{{{101}, DESCRIPTOR_SET}, {{1}, BUFFER}, {{2}, BUFFER}, {{3}, BUFFER}, {{201}, DESCRIPTOR_LAYOUT}, {{202}, DESCRIPTOR_POOL}}};
        const t::ResourcePlan<6> plan(requests);
        const auto all = inputs();
        const std::array<l::ResourceInit, 6> initializers{all[0], all[2], all[3], all[4], all[5], all[6]};
        std::optional<TypedSet> retained;
        std::weak_ptr<void> observed;
        {
            l::ResourceArena arena(device, plan, initializers, l::DependencyPolicy::DYNAMIC);
            observed = arena.owner();
            auto source = TypedSet::FromResource(device, arena.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
            auto [success, heap] = arena.descriptor_pool({202}).bind(arena.descriptor_layout({201}));
            ASSERT_TRUE(success);
            retained.emplace(TypedSet::FromResource(device, heap, vsl::ResourceRetention::COMPLETION));
            const auto logical = std::make_shared<vsl::ResourceUseState>();
            EXPECT_THROW(source.write<0>(UniformView::FromView(arena.buffer({1}), 1, 0, {logical, heap.owner_ref()})), std::invalid_argument);
            const auto physical = arena.buffer({1}).resource_use();
            EXPECT_THROW(source.write<0>(UniformView::FromView(arena.buffer({1}), 1, 0, {physical.state, heap.owner_ref()})), std::invalid_argument);
            EXPECT_THROW(source.write<0>(UniformView::FromView(arena.buffer({1}), 1, 0, {physical.state, heap.owner_ref().alias(static_cast<void*>(nullptr))})), std::invalid_argument);
            EXPECT_THROW(source.write<0>(UniformView::FromView(arena.buffer({1}), 1, 0, {heap.owner_ref().alias(static_cast<vsl::ResourceUseState*>(nullptr)), {}})), std::invalid_argument);
            retained->write<0>(UniformView::FromView(arena.buffer({1}), 1));
            retained->write<1>(StorageView::FromView(arena.buffer({2}), 1));
            EXPECT_TRUE(DescriptorAccess::Owned(retained->backend())->bindings.at(0).front().owners.empty());
            write_value(arena.buffer({1}), 97);
            auto pipeline = make_pipeline();
            EXPECT_EQ(execute_set(pipeline, make_manager().independent(1), *retained, arena.buffer({2})), 97u);
        }
        EXPECT_FALSE(observed.expired());
        retained.reset();
        EXPECT_TRUE(observed.expired());
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, ReplacedOwnerCallbacksSeeCommittedMetadataAndCanUpdateTheSameRoot) {
        const auto initializers = inputs();
        l::ResourceArena a(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        l::ResourceArena b(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto first = TypedSet::FromResource(device, a.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        auto second = TypedSet::FromResource(device, b.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        first.write<1>(StorageView::FromView(a.buffer({2}), 1));
        const auto state = std::make_shared<vsl::ResourceUseState>();
        size_t callbacks = 0;
        auto previous = std::shared_ptr<int>(new int(1), [&](int* value) {
            delete value;
            ++callbacks;
            EXPECT_TRUE(DescriptorAccess::Owned(first.backend())->bindings.at(0).front().constraint.states.empty());
            EXPECT_NO_THROW(first.write<1>(StorageView::FromView(b.buffer({2}), 1)));
            EXPECT_THROW(second.write<0>(UniformView::FromView(a.buffer({1}), 1)), std::invalid_argument);
        });
        first.write<0>(UniformView::FromView(a.buffer({1}), 1, 0, {state, previous}));
        previous.reset();
        EXPECT_EQ(callbacks, 0u);
        first.write<0>(UniformView::FromView(a.buffer({1}), 1));
        EXPECT_EQ(callbacks, 1u);
        write_value(a.buffer({1}), 61);
        auto pipeline = make_pipeline();
        EXPECT_EQ(execute_set(pipeline, make_manager().independent(1), first, b.buffer({2})), 61u);
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, NativePartialArrayUpdatesKeepTheUntouchedManagedEdgeUntilTheLastElementChanges) {
        using ArraySpec = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>;
        const std::array<t::ResourceRequest, 4> requests{{{{101}, DESCRIPTOR_SET}, {{1}, BUFFER}, {{201}, DESCRIPTOR_LAYOUT}, {{202}, DESCRIPTOR_POOL}}};
        const t::ResourcePlan<4> plan(requests);
        const std::array<l::ResourceInit, 4> initializers{
            l::DescriptorSetInit{{202}, {201}}, vsl::BufferCreateInfo{4, vsl::MemoryType::UNIFORM_BUFFER},
            vsl::DescriptorLayoutCreateInfo{{ArraySpec::POINTS.begin(), ArraySpec::POINTS.end()}},
            vsl::DescriptorPoolCreateInfo{{{vsl::graphic_resource::Type::UNIFORM_BUFFER, 2}}, 1}};
        l::ResourceArena a(device, plan, initializers, l::DependencyPolicy::DYNAMIC);
        const auto other_inputs = inputs();
        l::ResourceArena b(device, PLAN, other_inputs, l::DependencyPolicy::DYNAMIC);
        auto array = t::TypedDescriptorSet<ArraySpec>::FromResource(device, a.descriptor_set({101}));
        auto second = TypedSet::FromResource(device, b.descriptor_set({101}));
        const auto foreign = UniformView::FromView(b.buffer({1}), 1);
        array.write<0>(std::array{foreign, foreign});
        const auto logical = std::make_shared<vsl::ResourceUseState>();
        EXPECT_THROW(second.write<0>(UniformView::FromView(b.buffer({1}), 1, 0, {logical, a.owner_ref()})), std::invalid_argument);
        t::TypedUniformBuffer<std::uint32_t> replacement(device, std::uint32_t{7});
        auto native = array.backend();
        native.update(replacement.backend(), 0);
        EXPECT_EQ(DescriptorAccess::Owned(native)->bindings.at(0).size(), 2u);
        EXPECT_THROW(second.write<0>(UniformView::FromView(b.buffer({1}), 1, 0, {logical, a.owner_ref()})), std::invalid_argument);
        const auto local = UniformView::FromView(a.buffer({1}), 1);
        array.write<0>(std::array{local, local});
        EXPECT_NO_THROW(second.write<0>(UniformView::FromView(b.buffer({1}), 1, 0, {logical, a.owner_ref()})));
        EXPECT_THROW(array.write<0>(std::array{foreign, local}), std::invalid_argument);
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, ImportedLayoutIsAFixedManagedParentEvenBeforeItsSetsHaveBindings) {
        const auto initializers = inputs();
        l::ResourceArena a(device, PLAN, initializers, l::DependencyPolicy::DYNAMIC);
        auto requests = REQUESTS;
        requests[5].external = true;
        const t::ResourcePlan<7> plan(requests);
        auto imported = initializers;
        imported[5] = a.descriptor_layout({201});
        l::ResourceArena b(device, plan, imported, l::DependencyPolicy::DYNAMIC);
        auto first = TypedSet::FromResource(device, a.descriptor_set({101}));
        const auto logical = std::make_shared<vsl::ResourceUseState>();
        EXPECT_THROW(first.write<0>(UniformView::FromView(a.buffer({1}), 1, 0, {logical, b.owner_ref()})), std::invalid_argument);
        auto second = TypedSet::FromResource(device, b.descriptor_set({101}), vsl::ResourceRetention::COMPLETION);
        second.write<0>(UniformView::FromView(a.buffer({1}), 1));
        second.write<1>(StorageView::FromView(b.buffer({2}), 1));
        EXPECT_TRUE(DescriptorAccess::Owned(second.backend())->bindings.at(0).front().owners.empty());
        write_value(a.buffer({1}), 107);
        auto pipeline = make_pipeline();
        EXPECT_EQ(execute_set(pipeline, make_manager().independent(1), second, b.buffer({2})), 107u);
    }
} // namespace

namespace {
    TEST_F(DescriptorArenaVulkanIntegrationTest, RetiredOwnersCannotPublishDescriptorOwnership) {
        const auto initializers = inputs();
        l::ResourceArena arena(device, PLAN, initializers);
        const auto resource = arena.descriptor_set({101});
        const auto native = DescriptorAccess::Owned(resource)->descriptor_set;
        const_cast<l::DependencyNode*>(DescriptorAccess::Owned(resource)->owner_node)->retire();
        EXPECT_THROW(resource.owner_ref(), std::invalid_argument);
        EXPECT_EQ(DescriptorAccess::Owned(resource)->descriptor_set, native);
    }
    TEST_F(DescriptorArenaVulkanIntegrationTest, KnownCompositeNodesAreValidatedWithoutRestrictingOpaqueOwners) {
        struct Composite {
            l::DependencyNode node;
            l::OwnerRef source;
            explicit Composite(l::OwnerRef source) : source(std::move(source)) {}
            ~Composite() { node.retire(); }
            static void Visit(const void* context, l::DependencyNode::EdgeVisitor visitor, void* output) {
                visitor(output, static_cast<const Composite*>(context)->source);
            }
        };
        const auto initializers = inputs();
        l::ResourceArena arena(device, PLAN, initializers);
        const auto original = arena.descriptor_set({101});
        const auto native = DescriptorAccess::Owned(original)->descriptor_set;
        auto context = std::make_shared<Composite>(original.owner_ref());
        context->node.publish(context, context.get(), Composite::Visit);
        auto* target = DescriptorAccess::Owned(original).get();
        const auto composite = DescriptorAccess::FromOwned(context->node.ref(context).alias(target));
        const auto opaque = DescriptorAccess::FromShared(std::shared_ptr<vsl::impl::GraphicResourceImpl>(context, target));
        const auto exported = composite.owner_ref();
        EXPECT_TRUE(exported.managed());
        EXPECT_FALSE(exported.share().owner_before(context));
        EXPECT_FALSE(context.owner_before(exported.share()));
        EXPECT_FALSE(opaque.owner_ref().managed());
        context->node.retire();
        EXPECT_THROW(composite.owner_ref(), std::invalid_argument);
        EXPECT_FALSE(opaque.owner_ref().managed());
        EXPECT_TRUE(original.owner_ref().managed());
        EXPECT_EQ(DescriptorAccess::Owned(original)->descriptor_set, native);
    }
} // namespace
