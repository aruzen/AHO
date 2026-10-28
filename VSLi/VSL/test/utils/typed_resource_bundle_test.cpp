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
    void initialize_shared(Bundle& bundle);
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
    void initialize_shared(Bundle& bundle) {
        bundle.initialize_shared([](Bundle::Shared& shared) {
            const std::uint32_t bias = 10, first = 20, second = 30;
            shared.buffer<2, 1>().copy(std::span(&bias, 1));
            shared.buffer<5, 0, 0>().copy(std::span(&first, 1));
            shared.buffer<5, 0, 1>().copy(std::span(&second, 1));
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
