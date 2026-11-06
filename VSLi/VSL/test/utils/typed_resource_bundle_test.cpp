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
    class TypedBundleVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
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
    }

    TEST_F(TypedBundleVulkanIntegrationTest, ArenaResourcesReachTheShaderThroughArraysAndFrameReuse) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        ArenaBundle bundle(manager, descriptors, sizes());
        initialize_shared(bundle);
        vsl::Shader<vsl::ShaderType::COMPUTE> shader(device, vsl_test::shader_path("typed_bundle.comp.spv"));
        t::TypedComputePipeline<Resources> pipeline(device, vsl::pipeline_layout::ShaderGroup("arena bundle", {shader}));
        for (std::uint32_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = manager.start_phase<vsl::ComputePhase>();
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

    TEST_F(TypedBundleVulkanIntegrationTest, ArenaFailureUnwindsSetsAndCanBeRetried) {
        auto manager = make_manager().independent(3);
        CountingManager descriptors(device);
        descriptors.fail_after = 2;
        EXPECT_THROW((ArenaBundle(manager, descriptors, sizes())), std::runtime_error);
        for (const auto& record : descriptors.records)
            EXPECT_TRUE(record.expired());
        descriptors.fail_after = SIZE_MAX;
        EXPECT_NO_THROW((ArenaBundle(manager, descriptors, sizes())));
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
