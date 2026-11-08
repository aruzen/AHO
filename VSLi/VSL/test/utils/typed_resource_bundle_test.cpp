#include <VSL/utils/typed_resource_bundle.hpp>
#include <VSL/utils/typed_compute.hpp>
#include "support/vulkan_fixture.hpp"

namespace {
    namespace t = vsl::typed;
    using LocalSet = t::DescriptorSet<2, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::UniformBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                      t::StorageBinding<3, std::uint32_t, vsl::ShaderFlag::COMPUTE>>;
    using SharedSet = t::DescriptorSet<5, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE, 2>>;
    using Resources = t::ResourceSchema<LocalSet, SharedSet>;
    using Allocation = t::ResourceAllocationSchema<Resources, t::BufferAllocation<2, 0>,
                                                   t::BufferAllocation<2, 1, t::FixedElements<1>, t::ResourceScope::SHARED>,
                                                   t::BufferAllocation<2, 3, t::RuntimeElements>,
                                                   t::BufferAllocation<5, 0, t::FixedElements<1>, t::ResourceScope::SHARED>>;
    using Bundle = t::TypedResourceBundle<Allocation>;
    using ArenaBundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<>>;
    static_assert(Bundle::RETENTION == vsl::ResourceRetention::COMPLETION);
    static_assert(ArenaBundle::RETENTION == Bundle::RETENTION);
    static_assert(Bundle::DEPENDENCIES == vsl::lifetime::DependencyPolicy::FIXED);
    class TypedBundleVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    template<typename Storage_, vsl::ResourceRetention retention>
    struct BundleLifetimeCase {
        using Storage = Storage_;
        static constexpr auto RETENTION = retention;
    };
    using BundleLifetimeCases = ::testing::Types<
        BundleLifetimeCase<t::HeapStorage, vsl::ResourceRetention::COMPLETION>,
        BundleLifetimeCase<t::ArenaStorage<>, vsl::ResourceRetention::COMPLETION>,
        BundleLifetimeCase<t::HeapStorage, vsl::ResourceRetention::SUBMISSION>,
        BundleLifetimeCase<t::ArenaStorage<>, vsl::ResourceRetention::SUBMISSION>>;
    template<typename Case>
    class TypedBundleLifetimeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    class CountingManager : public vsl::GraphicResourceManager {
    public:
        using vsl::GraphicResourceManager::GraphicResourceManager;
        size_t allocations = 0, fail_after = std::numeric_limits<size_t>::max();
        std::vector<std::weak_ptr<vsl::impl::GraphicResourceImpl>> records;
        std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>> allocate(std::vector<vsl::graphic_resource::BindingLayout> layouts) override;
    };
    t::ResourceAllocationSizes<Allocation> sizes();
    template<typename B>
    void initialize_shared(B& bundle);
    void host_read_barrier(vsl::CommandManager manager, std::uint32_t slot);

    auto CountingManager::allocate(std::vector<vsl::graphic_resource::BindingLayout> layouts)
        -> std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>> {
        if (allocations >= fail_after)
            throw std::runtime_error("Injected descriptor allocation failure");
        auto result = vsl::GraphicResourceManager::allocate(std::move(layouts));
        for (const auto& resource : std::get<1>(result)) {
            ++allocations;
            records.push_back(resource.data_);
        }
        return result;
    }
    t::ResourceAllocationSizes<Allocation> sizes() {
        t::ResourceAllocationSizes<Allocation> result;
        result.set<2, 3>(4);
        return result;
    }
    template<typename B>
    void initialize_shared(B& bundle) {
        bundle.initialize_shared([](typename B::Shared& shared) {
            const std::uint32_t bias = 10, first = 20, second = 30;
            shared.template buffer<2, 1>().copy(std::span(&bias, 1));
            shared.template buffer<5, 0, 0>().copy(std::span(&first, 1));
            shared.template buffer<5, 0, 1>().copy(std::span(&second, 1));
        });
    }
    void host_read_barrier(vsl::CommandManager manager, std::uint32_t slot) {
        auto buffer = manager.get_buffer();
        vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        buffer.data_->command_buffers.at(slot).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
    }

    TEST_F(TypedBundleVulkanIntegrationTest, AllocationPlanMatchesRealSetsAndIndependentBufferOwners) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        Bundle bundle(manager, descriptors, sizes());
        EXPECT_EQ(bundle.plan().totals().buffers, 9u);
        EXPECT_EQ(bundle.plan().totals().descriptor_sets, 4u);
        EXPECT_EQ(bundle.plan().totals().descriptors, 11u);
        EXPECT_EQ(bundle.plan().totals().payload_bytes, 72u);
        EXPECT_EQ(descriptors.allocations, bundle.plan().totals().descriptor_sets);
        std::array<std::shared_ptr<vsl::impl::BufferImpl>, 3> local, shared;
        std::array<std::shared_ptr<vsl::impl::GraphicResourceImpl>, 3> local_sets, shared_sets;
        for (size_t slot = 0; slot < 3; ++slot) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            local[slot] = frame.buffer<2, 0>().backend().data_;
            shared[slot] = frame.buffer<2, 1>().backend().data_;
            local_sets[slot] = frame.set<2>().backend().data_;
            shared_sets[slot] = frame.set<5>().backend().data_;
            auto buffer = frame.buffer<2, 3>().backend();
            auto mapped = buffer.data();
            const auto* values = static_cast<const std::uint32_t*>(mapped.data);
            for (size_t i = 0; i < 4; ++i)
                EXPECT_EQ(values[i], 0u);
            EXPECT_NO_THROW(frame.set<2>().require_complete());
            EXPECT_NO_THROW(frame.set<5>().require_complete());
            phase.cancel();
            manager.get_buffer().next();
        }
        EXPECT_NE(local[0], local[1]);
        EXPECT_NE(local[1], local[2]);
        EXPECT_NE(local[0], local[2]);
        EXPECT_EQ(shared[0], shared[1]);
        EXPECT_EQ(shared[1], shared[2]);
        EXPECT_NE(local_sets[0], local_sets[1]);
        EXPECT_EQ(shared_sets[0], shared_sets[1]);
        EXPECT_EQ(shared_sets[1], shared_sets[2]);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ArenaPolicyPlacesBuffersContiguouslyWithinSeparateLifetimeGroups) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        ArenaBundle bundle(manager, descriptors, sizes());
        EXPECT_EQ(bundle.placement_plan().buffers(t::ResourceScope::SHARED).size(), 3u);
        EXPECT_EQ(bundle.placement_plan().buffers(t::ResourceScope::PER_FRAME).size(), 2u);
        std::array<const void*, 3> local{}, shared{};
        std::array<const void*, 3> layouts{}, pools{}, sets{};
        for (size_t slot = 0; slot < 3; ++slot) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            const auto& first = frame.buffer<2, 0>().backend();
            const auto& second = frame.buffer<2, 3>().backend();
            local[slot] = first.owner().get();
            shared[slot] = frame.buffer<2, 1>().backend().owner().get();
            EXPECT_EQ(first.owner(), second.owner());
            EXPECT_NE(local[slot], shared[slot]);
            EXPECT_EQ(reinterpret_cast<const std::byte*>(second.ref().data) - reinterpret_cast<const std::byte*>(first.ref().data), vsl::storage::BufferLayout::STRIDE);
            const auto& native = frame.set<2>().backend();
            const auto& shared_native = frame.set<5>().backend();
            const auto& plan = bundle.placement_plan();
            const auto physical = plan.arena_plan(t::ResourceScope::PER_FRAME, slot);
            const auto base = reinterpret_cast<std::uintptr_t>(first.ref().data) - plan.placement({2, 0}).offset;
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(native.data_.get()) - base, physical.placement(plan.descriptor_set(2)).offset);
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(native.data_->pool) - base, physical.placement(*plan.descriptor_pool(t::ResourceScope::PER_FRAME)).offset);
            const auto shared_base = reinterpret_cast<std::uintptr_t>(frame.buffer<2, 1>().backend().ref().data) - plan.placement({2, 1}).offset;
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(native.data_->layout) - shared_base, plan.arena_plan(t::ResourceScope::SHARED).placement(plan.descriptor_layout(2)).offset);
            EXPECT_FALSE(native.owner_ref().owner_before(first.owner_ref()));
            EXPECT_FALSE(first.owner_ref().owner_before(native.owner_ref()));
            EXPECT_FALSE(shared_native.owner_ref().owner_before(frame.buffer<2, 1>().backend().owner_ref()));
            EXPECT_FALSE((frame.buffer<2, 1>().backend().owner_ref().owner_before(shared_native.owner_ref())));
            EXPECT_EQ(native.data_->pool->max_sets, 1u);
            EXPECT_EQ(native.data_->pool->allocated_sets, 1u);
            EXPECT_EQ(native.data_->pool->capacities.at(vk::DescriptorType::eUniformBuffer).limit, 2u);
            EXPECT_EQ(native.data_->pool->capacities.at(vk::DescriptorType::eStorageBuffer).limit, 1u);
            for (const auto& [binding, elements] : native.data_->bindings)
                for (const auto& element : elements)
                    EXPECT_TRUE(element.owners.empty());
            layouts[slot] = native.data_->layout;
            pools[slot] = native.data_->pool;
            sets[slot] = native.data_.get();
            std::array<std::uint32_t, 4> zero{1, 1, 1, 1};
            frame.buffer<2, 3>().read_into(zero);
            EXPECT_EQ(zero, (std::array<std::uint32_t, 4>{}));
            auto slice = frame.buffer<2, 3>().view(1, 2);
            slice.copy(std::array<std::uint32_t, 2>{7, 9});
            frame.buffer<2, 3>().read_into(zero);
            EXPECT_EQ(zero, (std::array<std::uint32_t, 4>{0, 7, 9, 0}));
            EXPECT_EQ(slice.backend().owner(), second.owner());
            EXPECT_THROW(slice.copy(std::array<std::uint32_t, 1>{}), std::invalid_argument);
            EXPECT_THROW((frame.buffer<2, 3>().view(4, 1)), std::out_of_range);
            phase.cancel();
            manager.get_buffer().next();
        }
        EXPECT_NE(local[0], local[1]);
        EXPECT_NE(local[1], local[2]);
        EXPECT_EQ(shared[0], shared[1]);
        EXPECT_EQ(shared[1], shared[2]);
        EXPECT_EQ(layouts[0], layouts[1]);
        EXPECT_EQ(layouts[1], layouts[2]);
        EXPECT_NE(pools[0], pools[1]);
        EXPECT_NE(pools[1], pools[2]);
        EXPECT_NE(sets[0], sets[1]);
        EXPECT_NE(sets[1], sets[2]);
        EXPECT_EQ(descriptors.allocations, 0u);
    }
    TEST_F(TypedBundleVulkanIntegrationTest, DeclaredEmptySetUsesAnArenaAndEmptySchemaUsesNone) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        descriptors.fail_after = 0;
        using Empty = t::ResourceAllocationSchema<t::ResourceSchema<>>;
        t::TypedResourceBundle<Empty, t::ArenaStorage<>> empty(manager, descriptors);
        EXPECT_EQ(empty.placement_plan().total_arena_bytes(), 0u);
        empty.initialize_shared([](auto&) {});
        using Declared = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<500>>>;
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto completion = phase.completion();
        std::weak_ptr<void> root;
        {
            t::TypedResourceBundle<Declared, t::ArenaStorage<>> bundle(manager, descriptors);
            EXPECT_EQ(bundle.placement_plan().total_bytes(), 0u);
            EXPECT_GT(bundle.placement_plan().total_arena_bytes(), 0u);
            const auto& set = bundle.get(phase).set<500>();
            const auto& native = set.backend();
            root = native.data_;
            EXPECT_EQ(native.data_->pool->max_sets, 1u);
            EXPECT_TRUE(native.data_->pool->capacities.empty());
            EXPECT_NO_THROW(set.require_complete());
            set.resource_use()->record(manager.get_buffer());
        }
        EXPECT_FALSE(root.expired());
        phase.cancel();
        EXPECT_TRUE(root.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_EQ(descriptors.allocations, 0u);
    }
    TEST_F(TypedBundleVulkanIntegrationTest, FrameSetAloneRetainsSharedLayoutsWithoutRetainingOtherFramesOrTheFacade) {
        auto manager = make_manager().independent(2);
        CountingManager descriptors(device);
        std::optional<t::TypedDescriptorSet<LocalSet::Layout>> kept;
        std::weak_ptr<void> frame, shared, other;
        {
            ArenaBundle bundle(manager, descriptors, sizes());
            auto first = manager.start_phase<vsl::ComputePhase>();
            const auto& value = bundle.get(first);
            kept.emplace(value.set<2>());
            frame = value.buffer<2, 0>().backend().owner();
            shared = value.buffer<2, 1>().backend().owner();
            first.cancel();
            manager.get_buffer().next();
            auto next = manager.start_phase<vsl::ComputePhase>();
            other = bundle.get(next).buffer<2, 0>().backend().owner();
            next.cancel();
        }
        EXPECT_TRUE(other.expired());
        EXPECT_FALSE(frame.expired());
        EXPECT_FALSE(shared.expired());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto completion = phase.completion();
        EXPECT_NO_THROW(kept->require_complete());
        kept->resource_use()->record(manager.get_buffer());
        kept.reset();
        EXPECT_FALSE(frame.expired());
        EXPECT_FALSE(shared.expired());
        phase.cancel();
        EXPECT_TRUE(frame.expired());
        EXPECT_TRUE(shared.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
    }
    TEST_F(TypedBundleVulkanIntegrationTest, SharedSetRejectsAReverseDependencyIntoItsFrameArena) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        std::weak_ptr<void> local_owner, shared_owner;
        {
            ArenaBundle bundle(manager, descriptors, sizes());
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto& frame = bundle.get(phase);
            auto shared = frame.set<5>();
            const auto local = frame.buffer<2, 0>();
            local_owner = local.backend().owner();
            shared_owner = shared.backend().data_;
            EXPECT_TRUE(local.descriptor().buffer.managed());
            EXPECT_THROW(shared.write<0>(std::array{local, local}), std::invalid_argument);
            using External = t::detail::BundleExternalValue<SharedSet::Binding<0>>;
            for (const auto form : {0, 1, 2}) {
                auto value = frame.buffer<5, 0>().descriptor();
                const auto owner = frame.set<2>().backend().owner_ref();
                if (form == 2) {
                    value.use.state = owner.alias(value.use.state.get());
                    value.use.owner.reset();
                } else
                    value.use.owner = form ? owner.alias(static_cast<void*>(nullptr)) : owner;
                EXPECT_THROW(shared.write<0>(std::array{External{value}, External{value}}), std::invalid_argument);
            }
            const auto logical = std::make_shared<vsl::ResourceUseState>();
            EXPECT_THROW(shared.resource_use()->replace(0, {{logical, frame.set<2>().backend().owner_ref()}, {}}), std::invalid_argument);
            EXPECT_NO_THROW(shared.require_complete());
            EXPECT_TRUE(shared.backend().data_->bindings.at(0).front().constraint.states.empty());
            phase.cancel();
        }
        EXPECT_TRUE(local_owner.expired());
        EXPECT_TRUE(shared_owner.expired());
    }
    TEST_F(TypedBundleVulkanIntegrationTest, FailedInitialArrayBindingUnwindsTheArenaAndExternalOwnersBeforeRetry) {
        using Layout = t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                        t::StorageBinding<1, std::uint32_t, vsl::ShaderFlag::COMPUTE>,
                                        t::TextureBinding<2, vsl::ShaderFlag::COMPUTE, 2>>;
        using Allocation = t::ResourceAllocationSchema<t::ResourceSchema<Layout>, t::ExternalAllocation<0, 0>,
                                                       t::BufferAllocation<0, 1>, t::ExternalAllocation<0, 2>>;
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        t::TypedUniformBuffer<std::uint32_t> uniform(device, std::uint32_t{37});
        vsl::Image<vsl::ImageType::SAMPLED> valid(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Image<vsl::ImageType::COLOR_ATTACHMENT> invalid(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Sampler sampler(device);
        const auto uniform_input = t::external_resource<0, 0>(uniform);
        auto textures = t::external_resource<0, 2>(std::array{t::TypedTexture(valid, sampler), t::TypedTexture(invalid, sampler)});
        const auto uniform_refs = uniform.backend().data_.use_count();
        const auto image_refs = valid.data_.use_count();
        const auto sampler_refs = sampler.data_.use_count();
        using Bundle = t::TypedResourceBundle<Allocation, t::ArenaStorage<>>;
        EXPECT_THROW((Bundle(manager, descriptors, uniform_input, textures)), std::invalid_argument);
        EXPECT_EQ(uniform.backend().data_.use_count(), uniform_refs);
        EXPECT_EQ(valid.data_.use_count(), image_refs);
        EXPECT_EQ(sampler.data_.use_count(), sampler_refs);
        textures.values[1] = t::TypedTexture(valid, sampler);
        EXPECT_NO_THROW((Bundle(manager, descriptors, uniform_input, textures)));
        EXPECT_EQ(descriptors.allocations, 0u);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ArenaResourcesReachTheShaderThroughArraysAndFrameReuse) {
        t::TypedFrameCommands commands(device, 3, t::ArenaStorage<>{});
        auto manager = commands.manager();
        CountingManager descriptors(device);
        ArenaBundle bundle(manager, descriptors, sizes());
        initialize_shared(bundle);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, bundle.descriptor_layouts(), vsl::pipeline_layout::ShaderGroup("arena bundle", {shader}));
        for (std::uint32_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = commands.start_compute();
            bundle.update(phase, [&](ArenaBundle::Frame& frame) {
                frame.buffer<2, 0>().copy(std::span(&iteration, 1));
            });
            const auto& frame = bundle.get(phase);
            phase << frame.bind_resources(pipeline).dispatch(4);
            EXPECT_THROW((frame.buffer<2, 3>().read_into(std::span<std::uint32_t>{})), std::invalid_argument);
            std::array<std::uint32_t, 4> result{};
            EXPECT_THROW((frame.buffer<2, 3>().read_into(result)), std::logic_error);
            host_read_barrier(manager, phase.frame_slot);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            frame.buffer<2, 3>().read_into(result);
            for (size_t i = 0; i < result.size(); ++i)
                EXPECT_EQ(result[i], iteration + 60u + i);
            commands.next();
        }
    }

    TEST_F(TypedBundleVulkanIntegrationTest, OneResourceCanServeMultipleBindingsWithoutDuplicateStorage) {
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{5, 0, 0}, t::ResourceId{42}>,
                                        t::ResourceBinding<t::BindingKey{5, 0, 1}, t::ResourceId{42}>>;
        using Aliased = t::TypedResourceBundle<Allocation, t::ArenaStorage<Map>>;
        auto manager = make_manager();
        CountingManager descriptors(device);
        Aliased bundle(manager, descriptors, sizes());
        initialize_shared(bundle);
        EXPECT_EQ(bundle.placement_plan().buffers(t::ResourceScope::SHARED).size(), 2u);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto& frame = bundle.get(phase);
        EXPECT_EQ((frame.buffer<5, 0, 0>().backend().ref().data), (frame.buffer<5, 0, 1>().backend().ref().data));
        std::array<std::uint32_t, 1> value{};
        frame.buffer<5, 0, 0>().read_into(value);
        EXPECT_EQ(value[0], 30u);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, CancelReleasesFrameArenaWhileSharedViewAndCompletionRemainAlive) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto completion = phase.completion();
        std::weak_ptr<void> local, shared;
        std::optional<t::PlacedResourceBufferView<std::uint32_t, vsl::graphic_resource::Type::UNIFORM_BUFFER>> kept;
        {
            ArenaBundle bundle(manager, descriptors, sizes());
            const auto& frame = bundle.get(phase);
            local = frame.buffer<2, 0>().backend().owner();
            shared = frame.buffer<2, 1>().backend().owner();
            kept.emplace(frame.buffer<2, 1>());
            frame.set<2>().resource_use()->record(manager.get_buffer());
        }
        EXPECT_FALSE(local.expired());
        phase.cancel();
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(local.expired());
        EXPECT_FALSE(shared.expired());
        kept.reset();
        EXPECT_TRUE(shared.expired());
    }

    TEST_F(TypedBundleVulkanIntegrationTest, SubmittedArenaBundleIsReleasedAfterGpuCompletionWithSavedCompletion) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("arena lifetime", {shader}));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto completion = phase.completion();
        std::weak_ptr<void> owner;
        {
            ArenaBundle bundle(manager, descriptors, sizes());
            const auto& frame = bundle.get(phase);
            owner = frame.buffer<2, 3>().backend().owner();
            phase << frame.bind_resources(pipeline).dispatch(4);
        }
        EXPECT_FALSE(owner.expired());
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(completion.wait());
        EXPECT_TRUE(owner.expired());
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ArenaDescriptorsUseTheirPlannedPoolsWithoutLegacyManagerAllocation) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        descriptors.fail_after = 0;
        EXPECT_NO_THROW((ArenaBundle(manager, descriptors, sizes())));
        EXPECT_EQ(descriptors.allocations, 0u);
        EXPECT_TRUE(descriptors.records.empty());
    }

    TEST_F(TypedBundleVulkanIntegrationTest, CommandKeepsArenaAfterOriginalBundleAndRecordingPhaseAreGone) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("deferred arena", {shader}));
        std::weak_ptr<void> owner;
        vsl::Completion completed;
        {
            auto command = [&] {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                ArenaBundle bundle(manager, descriptors, sizes());
                const auto& frame = bundle.get(phase);
                owner = frame.buffer<2, 3>().backend().owner();
                auto result = frame.bind_resources(pipeline).dispatch(4);
                phase.cancel();
                return result;
            }();
            EXPECT_FALSE(owner.expired());
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << command;
            completed = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(completed.wait());
            EXPECT_FALSE(owner.expired());
        }
        EXPECT_TRUE(owner.expired());
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ExternalBufferUsesNoSlotAndSharesPhysicalUpdateConstraints) {
        using External = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>>, t::ExternalAllocation<0, 0>>;
        auto manager = make_manager();
        CountingManager descriptors(device);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto completion = phase.completion();
        std::weak_ptr<vsl::impl::BufferImpl> owner;
        {
            t::TypedUniformBuffer<std::uint32_t> external(device, 7u);
            owner = external.backend().data_;
            t::TypedResourceBundle<External, t::ArenaStorage<>> bundle(manager, descriptors, t::external_resource<0, 0>(external));
            EXPECT_EQ(bundle.placement_plan().total_bytes(), 0u);
            bundle.get(phase).set<0>().resource_use()->record(manager.get_buffer());
            const std::uint32_t value = 8;
            EXPECT_THROW(external.copy(std::span(&value, 1)), std::logic_error);
        }
        EXPECT_FALSE(owner.expired());
        phase.cancel();
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, InvalidResourceAliasFailsBeforeAnyDescriptorAllocation) {
        using Map = t::BufferBindingMap<t::ResourceBinding<t::BindingKey{2, 0}, t::ResourceId{42}>,
                                        t::ResourceBinding<t::BindingKey{2, 1}, t::ResourceId{42}>>;
        auto manager = make_manager();
        CountingManager descriptors(device);
        EXPECT_THROW((t::TypedResourceBundle<Allocation, t::ArenaStorage<Map>>(manager, descriptors, sizes())), std::invalid_argument);
        EXPECT_EQ(descriptors.allocations, 0u);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, AutomaticallyConnectedSparseSetsAndArraysReachTheShaderAcrossSlotReuse) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        Bundle bundle(manager, descriptors, sizes());
        initialize_shared(bundle);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("bundle", {shader}));
        std::array<std::uint32_t, 3> expected{};
        for (std::uint32_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            const auto slot = phase.frame_slot;
            bundle.update(phase, [&](Bundle::Frame& frame) {
                auto mapped = frame.buffer<2, 3>().backend().data();
                const auto* output = static_cast<const std::uint32_t*>(mapped.data);
                for (size_t index = 0; index < 4; ++index)
                    EXPECT_EQ(output[index], expected[slot] ? expected[slot] + index : 0);
                const auto value = iteration + 100;
                frame.buffer<2, 0>().copy(std::span(&value, 1));
                expected[slot] = value + 60;
            });
            const auto& frame = bundle.get(phase);
            phase << frame.bind_resources(pipeline).dispatch(4);
            host_read_barrier(manager, slot);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            manager.get_buffer().next();
        }
        for (size_t slot = 0; slot < 3; ++slot) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto buffer = bundle.get(phase).buffer<2, 3>().backend();
            auto mapped = buffer.data();
            const auto* output = static_cast<const std::uint32_t*>(mapped.data);
            for (size_t index = 0; index < 4; ++index)
                EXPECT_EQ(output[index], expected[slot] + index);
            phase.cancel();
            manager.get_buffer().next();
        }
        EXPECT_EQ(descriptors.allocations, 4u);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, SharedInitializationIsOnceBeforeAnyFrameAccess) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        Bundle bundle(manager, descriptors, sizes());
        initialize_shared(bundle);
        EXPECT_THROW(initialize_shared(bundle), std::logic_error);
        Bundle initially_zero(manager, descriptors, sizes());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        initially_zero.get(phase);
        EXPECT_THROW(initialize_shared(initially_zero), std::logic_error);
        bundle.update(phase, [](Bundle::Frame&) {});
        EXPECT_THROW(bundle.update(phase, [](Bundle::Frame&) {}), std::logic_error);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, SharedInitializationFailureAndReentryCannotExposePartialState) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        Bundle bundle(manager, descriptors, sizes());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto failed_initialize = [&](Bundle::Shared&) {
            EXPECT_THROW(bundle.get(phase), std::logic_error);
            EXPECT_THROW(initialize_shared(bundle), std::logic_error);
            throw std::runtime_error("Initialization failed");
        };
        EXPECT_THROW(bundle.initialize_shared(failed_initialize), std::runtime_error);
        EXPECT_THROW(bundle.get(phase), std::logic_error);
        EXPECT_THROW(bundle.update(phase, [](Bundle::Frame&) {}), std::logic_error);
        EXPECT_THROW(initialize_shared(bundle), std::logic_error);
    }

    TEST_F(TypedBundleVulkanIntegrationTest, InvalidSizesAndDeviceLimitsFailBeforeDescriptorAllocation) {
        auto manager = make_manager();
        CountingManager descriptors(device);
        EXPECT_THROW((Bundle(manager, descriptors)), std::invalid_argument);
        EXPECT_EQ(descriptors.allocations, 0u);
        using Limited = t::ResourceAllocationSchema<t::ResourceSchema<t::DescriptorSet<0, t::UniformBinding<0, std::uint32_t, vsl::ShaderFlag::COMPUTE>>>, t::BufferAllocation<0, 0, t::RuntimeElements>>;
        t::ResourceAllocationSizes<Limited> capacity;
        capacity.set<0, 0>(size_t(device.data_->parent_device->device.getProperties().limits.maxUniformBufferRange) / 4 + 1);
        EXPECT_THROW((t::TypedResourceBundle<Limited>(manager, descriptors, capacity)), std::out_of_range);
        EXPECT_EQ(descriptors.allocations, 0u);
        const auto identity = descriptors.data_;
        descriptors.data_.reset();
        EXPECT_THROW((Bundle(manager, descriptors, sizes())), std::invalid_argument);
        descriptors.data_ = identity;
    }

    TEST_F(TypedBundleVulkanIntegrationTest, PartialConstructionReleasesPreviouslyAllocatedSets) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        descriptors.fail_after = 2;
        EXPECT_THROW((Bundle(manager, descriptors, sizes())), std::runtime_error);
        EXPECT_EQ(descriptors.allocations, 2u);
        for (const auto& record : descriptors.records)
            EXPECT_TRUE(record.expired());
        descriptors.fail_after = std::numeric_limits<size_t>::max();
        Bundle next(manager, descriptors, sizes());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_NO_THROW(next.get(phase));
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ExternalTextureArraysAreConnectedOnceAndRetainImagesAndSamplers) {
        using Textures = t::ResourceSchema<t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT, 2>>>;
        using External = t::ResourceAllocationSchema<Textures, t::ExternalAllocation<1, 2>>;
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        std::weak_ptr<vsl::impl::ImageImpl> owner;
        auto bundle = [&] {
            vsl::Image<vsl::ImageType::SAMPLED> first(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
            vsl::Image<vsl::ImageType::SAMPLED> second(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
            vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
            owner = second.data_;
            return t::TypedResourceBundle<External>(manager, descriptors,
                                                    t::external_resource<1, 2>(std::array{t::TypedTexture(first, sampler), t::TypedTexture(second, sampler)}));
        }();
        EXPECT_FALSE(owner.expired());
        EXPECT_EQ(bundle.plan().totals().buffers, 0u);
        EXPECT_EQ(descriptors.allocations, 1u);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_NO_THROW(bundle.get(phase).set<1>().require_complete());
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ExternalImagesStillValidateUsageBeforeDescriptorWrites) {
        using Textures = t::ResourceSchema<t::DescriptorSet<1, t::TextureBinding<2, vsl::ShaderFlag::FRAGMENT>>>;
        using External = t::ResourceAllocationSchema<Textures, t::ExternalAllocation<1, 2>>;
        auto manager = make_manager();
        CountingManager descriptors(device);
        vsl::Image<vsl::ImageType::COLOR_ATTACHMENT> image(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        EXPECT_THROW((t::TypedResourceBundle<External>(manager, descriptors, t::external_resource<1, 2>(t::TypedTexture(image, sampler)))), std::invalid_argument);
        for (const auto& record : descriptors.records)
            EXPECT_TRUE(record.expired());
    }

    TEST_F(TypedBundleVulkanIntegrationTest, MoveAndContainerDestructionPreserveTheActiveFrameOwners) {
        auto manager = make_manager().independent(1);
        CountingManager descriptors(device);
        std::weak_ptr<vsl::impl::BufferImpl> owner;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                Bundle original(manager, descriptors, sizes());
                owner = original.get(phase).buffer<2, 0>().backend().data_;
                auto moved = std::move(original);
                EXPECT_THROW(original.get(phase), std::logic_error);
                EXPECT_NO_THROW(moved.get(phase));
            }
            EXPECT_FALSE(owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        EXPECT_TRUE(owner.expired());
    }
} // namespace

namespace {
    TYPED_TEST_SUITE(TypedBundleLifetimeVulkanIntegrationTest, BundleLifetimeCases);

    TYPED_TEST(TypedBundleLifetimeVulkanIntegrationTest, CompletionRetentionIsIndependentOfPlacement) {
        using B = t::TypedResourceBundle<Allocation, typename TypeParam::Storage, TypeParam::RETENTION>;
        auto manager = this->make_manager();
        CountingManager descriptors(this->device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(this->device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(this->device, vsl::pipeline_layout::ShaderGroup("retention", {shader}));
        vsl::Completion completion;
        std::weak_ptr<void> owner, shared;
        {
            auto phase = manager.template start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                B bundle(manager, descriptors, sizes());
                const auto& frame = bundle.get(phase);
                owner = frame.template set<2>().backend().data_;
                shared = frame.template set<5>().backend().data_;
                phase << frame.bind_resources(pipeline).dispatch(4);
            }
            EXPECT_FALSE(owner.expired());
            EXPECT_FALSE(shared.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(completion.wait());
            EXPECT_EQ(owner.expired(), TypeParam::RETENTION == vsl::ResourceRetention::COMPLETION);
            EXPECT_EQ(shared.expired(), TypeParam::RETENTION == vsl::ResourceRetention::COMPLETION);
        }
        auto next = manager.template start_phase<vsl::ComputePhase>();
        ASSERT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(next.completion().wait());
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(shared.expired());
    }
    TYPED_TEST(TypedBundleLifetimeVulkanIntegrationTest, CancellationUsesTheSameRetentionContractForBothPlacements) {
        using B = t::TypedResourceBundle<Allocation, typename TypeParam::Storage, TypeParam::RETENTION>;
        auto manager = this->make_manager();
        CountingManager descriptors(this->device);
        vsl::Completion completion;
        std::weak_ptr<void> owner, shared;
        {
            auto phase = manager.template start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                B bundle(manager, descriptors, sizes());
                const auto& frame = bundle.get(phase);
                owner = frame.template set<2>().backend().data_;
                shared = frame.template set<5>().backend().data_;
                frame.template set<2>().resource_use()->record(manager.get_buffer());
                frame.template set<5>().resource_use()->record(manager.get_buffer());
            }
            EXPECT_FALSE(owner.expired());
            EXPECT_FALSE(shared.expired());
            phase.cancel();
            EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
            EXPECT_EQ(owner.expired(), TypeParam::RETENTION == vsl::ResourceRetention::COMPLETION);
            EXPECT_EQ(shared.expired(), TypeParam::RETENTION == vsl::ResourceRetention::COMPLETION);
        }
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(shared.expired());
    }
    TYPED_TEST(TypedBundleLifetimeVulkanIntegrationTest, SavedCommandRetainsResourcesAndCanBeRecordedAgain) {
        using B = t::TypedResourceBundle<Allocation, typename TypeParam::Storage, TypeParam::RETENTION>;
        auto manager = this->make_manager();
        CountingManager descriptors(this->device);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(this->device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(this->device, vsl::pipeline_layout::ShaderGroup("saved command", {shader}));
        std::weak_ptr<void> owner;
        vsl::Completion saved;
        {
            const auto command = [&] {
                auto phase = manager.template start_phase<vsl::ComputePhase>();
                B bundle(manager, descriptors, sizes());
                const auto& frame = bundle.get(phase);
                owner = frame.template set<2>().backend().data_;
                auto result = frame.bind_resources(pipeline).dispatch(4);
                phase.cancel();
                return result;
            }();
            for (unsigned repeat = 0; repeat < 2; ++repeat) {
                auto phase = manager.template start_phase<vsl::ComputePhase>();
                phase << command;
                saved = phase.completion();
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(saved.wait());
                EXPECT_FALSE(owner.expired());
            }
        }
        EXPECT_EQ(owner.expired(), TypeParam::RETENTION == vsl::ResourceRetention::COMPLETION);
        auto next = manager.template start_phase<vsl::ComputePhase>();
        ASSERT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(next.completion().wait());
        EXPECT_TRUE(owner.expired());
    }
    TYPED_TEST(TypedBundleLifetimeVulkanIntegrationTest, FixedDependenciesRejectNewOwnersAndDynamicIsExplicit) {
        using B = t::TypedResourceBundle<Allocation, typename TypeParam::Storage, TypeParam::RETENTION>;
        using Dynamic = t::TypedResourceBundle<Allocation, typename TypeParam::Storage, TypeParam::RETENTION, vsl::lifetime::DependencyPolicy::DYNAMIC>;
        auto manager = this->make_manager();
        CountingManager descriptors(this->device);
        t::TypedUniformBuffer<std::uint32_t> other(this->device, 9u);
        auto phase = manager.template start_phase<vsl::ComputePhase>();
        B fixed(manager, descriptors, sizes());
        const auto& frame = fixed.get(phase);
        auto set = frame.template set<2>();
        EXPECT_THROW(set.template write<0>(other), std::invalid_argument);
        EXPECT_NO_THROW(set.template write<0>(frame.template buffer<2, 0>()));
        EXPECT_NO_THROW(set.require_complete());
        Dynamic dynamic(manager, descriptors, sizes());
        auto editable = dynamic.get(phase).template set<2>();
        EXPECT_NO_THROW(editable.template write<0>(other));
        EXPECT_NO_THROW(editable.require_complete());
        phase.cancel();
    }
} // namespace
