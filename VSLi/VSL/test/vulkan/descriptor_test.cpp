#include <VSL/lifetime/command_access.hpp>
#include "support/compute_fixture.hpp"
#include "support/scoped_environment.hpp"
#include <VSL/utils/typed_safe_resource.hpp>
#include <VSL/utils/typed_resources.hpp>
#include <VSL/lifetime/descriptor_storage.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    enum class ImageReadBarrierMode {
        AUTOMATIC,
        COMPUTE,
        TYPED,
        MULTI_STAGE,
        ALL_GRAPHICS_COMPUTE
    };
    using SampledTexture = vsl::typed::ResourceBindingPoint<2, vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, vsl::ShaderFlag::COMPUTE>;
    using SampledImage = vsl::Image<vsl::ImageType::SAMPLED | vsl::ImageType::TRANSFER_DST>;
    vsl::command::ChangeImageBarrier make_shader_read_barrier(ImageReadBarrierMode mode, const SampledImage& image);

    using DescriptorVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    class DescriptorImageBarrierVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest,
                                                        public testing::WithParamInterface<ImageReadBarrierMode> {
        vsl_test::ScopedEnvironment _synchronization{"VK_LAYER_VALIDATE_SYNC", "1"};
        vsl_test::ScopedEnvironment _shader_accesses{"VK_LAYER_SYNCVAL_SHADER_ACCESSES_HEURISTIC", "1"};
        bool validate_vulkan() const override;
    };

    class DescriptorOwnershipVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    vsl::command::ChangeImageBarrier make_shader_read_barrier(ImageReadBarrierMode mode, const SampledImage& image) {
        using namespace vsl;
        constexpr auto old_layout = ImageLayout::TRANSFER_DST_OPTIMAL;
        constexpr auto new_layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        switch (mode) {
            case ImageReadBarrierMode::AUTOMATIC:
                return {image, old_layout, new_layout};
            case ImageReadBarrierMode::COMPUTE:
                return {image, old_layout, new_layout, ShaderFlag::COMPUTE};
            case ImageReadBarrierMode::TYPED:
                return SampledTexture::MakeShaderReadBarrier(image);
            case ImageReadBarrierMode::MULTI_STAGE:
                return {image, old_layout, new_layout, ShaderFlag::VERTEX | ShaderFlag::COMPUTE};
            case ImageReadBarrierMode::ALL_GRAPHICS_COMPUTE:
                return {image, old_layout, new_layout, ShaderFlag::ALL_GRAPHICS | ShaderFlag::COMPUTE};
        }
        throw std::logic_error("Unknown shader read barrier mode");
    }

    bool DescriptorImageBarrierVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    bool DescriptorOwnershipVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, OrdinaryScopeExitReleasesPoolAndResourceWithoutDestroy) {
        std::weak_ptr<vsl::impl::GraphicResourcePoolImpl> weak_pool;
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_resource;
        {
            vsl::GraphicResourceManager manager(device);
            vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
            auto [pool, allocated] = manager.make(std::vector{layout});
            weak_pool = pool.data_;
            weak_resource = allocated.front().data_;
        }
        EXPECT_TRUE(weak_resource.expired());
        EXPECT_TRUE(weak_pool.expired());
    }
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, ResourceKeepsItsLayoutUntilTheLastAliasIsReleased) {
        vsl::GraphicResourceManager manager(device);
        vsl::GraphicResource retained;
        std::weak_ptr<vsl::impl::ResourceBindingLayoutImpl> weak_layout;
        {
            vsl::graphic_resource::BindingLayout layout(device, vsl::graphic_resource::Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
            weak_layout = layout.data_;
            auto [pool, allocated] = manager.make(std::vector{layout});
            retained = allocated.front();
        }
        EXPECT_FALSE(weak_layout.expired());
        retained = {};
        EXPECT_TRUE(weak_layout.expired());
    }
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, PlacedDescriptorBackendReusesFailedStorageAndConstructsDistinctSetSlots) {
        namespace l = vsl::lifetime;
        namespace op = vsl::operation;
        using Type = vsl::graphic_resource::Type;
        l::DescriptorLayoutSlotStorage layout_slot;
        l::DescriptorPoolSlotStorage pool_slot;
        std::array<l::DescriptorSetSlotStorage, 2> set_slots;
        const vsl::DescriptorLayoutCreateInfo layout_info{{{0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}}};
        auto invalid_layout = layout_info;
        invalid_layout.bindings.push_back(invalid_layout.bindings.front());
        EXPECT_THROW(l::construct_descriptor_layout(&layout_slot, device, invalid_layout), std::invalid_argument);
        l::construct_descriptor_layout(&layout_slot, device, layout_info);
        std::unique_ptr<void, void (*)(void*)> layout_guard(&layout_slot, l::destroy_descriptor_layout);
        vsl::DescriptorPoolCreateInfo pool_info{{{Type::STORAGE_BUFFER, 2}}, 0};
        EXPECT_THROW(l::construct_descriptor_pool(&pool_slot, device, pool_info), std::out_of_range);
        pool_info.max_sets = 2;
        l::construct_descriptor_pool(&pool_slot, device, pool_info);
        std::unique_ptr<void, void (*)(void*)> pool_guard(&pool_slot, l::destroy_descriptor_pool);
        auto& layout = l::descriptor_layout_at(&layout_slot);
        auto& pool = l::descriptor_pool_at(&pool_slot);
        l::prepare_descriptor_set(&set_slots[0], pool, layout);
        std::unique_ptr<void, void (*)(void*)> first_guard(&set_slots[0], l::destroy_descriptor_set);
        auto& first = l::descriptor_set_at(&set_slots[0]);
        const std::array duplicate{&first, &first};
        EXPECT_THROW(op::allocate_descriptor_sets(pool, duplicate), std::invalid_argument);
        EXPECT_FALSE(first.descriptor_set);
        const std::array prepared{&first};
        ASSERT_EQ(op::allocate_descriptor_sets(pool, prepared), vk::Result::eSuccess);
        l::construct_descriptor_set(&set_slots[1], pool, layout);
        std::unique_ptr<void, void (*)(void*)> second_guard(&set_slots[1], l::destroy_descriptor_set);
        auto& second = l::descriptor_set_at(&set_slots[1]);
        EXPECT_NE(&first, &second);
        EXPECT_NE(first.descriptor_set, second.descriptor_set);
        EXPECT_EQ(first.pool, &pool);
        EXPECT_EQ(second.layout, &layout);
        EXPECT_FALSE(first.use.busy());
        EXPECT_FALSE(second.use.busy());
    }
    TEST(BindingPointTest, ComparisonIncludesBindingTypeStageAndAmount) {
        using namespace vsl::graphic_resource;
        BindingPoint first{2, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1};
        EXPECT_EQ(first, (BindingPoint{2, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}));
        EXPECT_NE(first, (BindingPoint{3, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}));
        EXPECT_EQ(static_cast<unsigned>(Type::STORAGE_BUFFER),
                  static_cast<unsigned>(vk::DescriptorType::eStorageBuffer));
        EXPECT_EQ(static_cast<unsigned>(BindingDestination::COMPUTE),
                  static_cast<unsigned>(vk::PipelineBindPoint::eCompute));
    }
    TEST_F(DescriptorVulkanIntegrationTest, VectorAndInitializerLayoutsPreservePointsAndAutomaticNumbers) {
        using namespace vsl::graphic_resource;
        const std::vector<BindingPoint> points{{7, Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX, 2},
                                               {UINT32_MAX, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}};
        BindingLayout layout(device, points);
        auto expected = points;
        expected[1].binding = 1;
        EXPECT_EQ(layout.get_binding_points(), expected);
        EXPECT_EQ(binding->get_binding_points(),
                  (std::vector<BindingPoint>{{0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}}));
    }
    TEST_F(DescriptorVulkanIntegrationTest, ResourcePoolAndManagerIdentityArePreserved) {
        EXPECT_EQ(pool.get_manager(), &*resources);
        EXPECT_EQ(resource.get_pool().data_, pool.data_);
        ExpectSequence(execute(17), 17);
    }
    TEST_F(DescriptorVulkanIntegrationTest, PoolGetterUsesTheCanonicalOwnerAndDoesNotRetainItsSourceSet) {
        auto [created, allocated] = resources->make(std::vector{*binding});
        auto retained = allocated.front().get_pool();
        EXPECT_FALSE(retained.data_.owner_before(created.data_));
        EXPECT_FALSE(created.data_.owner_before(retained.data_));
        const std::weak_ptr<vsl::impl::GraphicResourceImpl> old = allocated.front().data_;
        allocated.clear();
        EXPECT_TRUE(old.expired());
        auto [success, rebound] = retained.bind(*binding);
        ASSERT_TRUE(success);
        rebound.update(&*output);
        resource = rebound;
        ExpectSequence(execute(19), 19);
    }
    TEST_F(DescriptorVulkanIntegrationTest, BindsSingleAndRepeatedLayoutsWithExplicitTypeUpdates) {
        using namespace vsl::graphic_resource;
        auto [single_ok, single] = pool.bind(*binding);
        ASSERT_TRUE(single_ok);
        ASSERT_TRUE(single.data_);
        single.update(&*output, 0, Type::STORAGE_BUFFER, 0, COUNT * sizeof(std::uint32_t));
        auto [many_ok, many] = pool.bind(2, *binding);
        ASSERT_TRUE(many_ok);
        ASSERT_EQ(many.size(), 2u);
        EXPECT_NE(many[0].data_->descriptor_set, many[1].data_->descriptor_set);
        EXPECT_EQ(many[0].get_pool().data_, pool.data_);
        EXPECT_EQ(single.data_->layout, binding->data_.get());
        EXPECT_NO_THROW(single.update(&*output));
        for (auto& entry : many) {
            EXPECT_EQ(entry.data_->layout, binding->data_.get());
            EXPECT_NO_THROW(entry.update(&*output));
        }
        resource = single;
        ExpectSequence(execute(23), 23);
    }
    TEST_F(DescriptorVulkanIntegrationTest, ValueUpdatePreservesBufferDataAndRange) {
        resource.update(*output, 0, vsl::graphic_resource::Type::STORAGE_BUFFER, 0, COUNT * sizeof(std::uint32_t));
        ExpectSequence(execute(31), 31);
    }
    TEST_F(DescriptorVulkanIntegrationTest, BulkNativeAllocationFailureRollsBackPreparedOwnersAndAllowsRetry) {
        const auto before = pool.data_->resources.size();
        const auto owners_before = pool.data_.use_count();
        const std::array layouts{binding->data_, binding->data_};
        const auto failed_allocate = [](vsl::impl::GraphicResourcePoolImpl&, std::span<vsl::impl::GraphicResourceImpl* const> sets) -> vk::Result {
            EXPECT_EQ(sets.size(), 2u);
            for (const auto* resource : sets)
                EXPECT_FALSE(resource->descriptor_set);
            return vk::Result::eErrorOutOfPoolMemory;
        };
        auto [failed, empty] = vsl::lifetime::make_heap_descriptor_sets(pool.data_, layouts, failed_allocate);
        EXPECT_EQ(failed, vk::Result::eErrorOutOfPoolMemory);
        EXPECT_TRUE(empty.empty());
        EXPECT_EQ(pool.data_->resources.size(), before);
        EXPECT_EQ(pool.data_.use_count(), owners_before);
        auto [success, allocated] = pool.bind(2, *binding);
        ASSERT_TRUE(success);
        ASSERT_EQ(allocated.size(), 2u);
        allocated.back().update(&*output);
        resource = allocated.back();
        ExpectSequence(execute(37), 37);
    }
    TEST_F(DescriptorVulkanIntegrationTest, SingleBindingConstructorsRetainDeviceOwner) {
        using namespace vsl::graphic_resource;
        auto imported = vsl::LogicalDeviceAccessor::Create(physical);
        const std::weak_ptr<vsl::impl::LogicalDeviceImpl> observed = imported.data_;
        const auto* native_owner = imported.data_.get();
        BindingLayout explicit_binding(imported, 2, Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX, 1);
        EXPECT_EQ(explicit_binding.data_->device, native_owner);
        EXPECT_EQ(explicit_binding.get_binding_points(),
                  (std::vector<BindingPoint>{{2, Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX, 1}}));
        BindingLayout automatic(imported, Type::UNIFORM_BUFFER, vsl::ShaderFlag::VERTEX);
        EXPECT_EQ(automatic.data_->device, native_owner);
        EXPECT_EQ(automatic.get_binding_points().front().binding, 0u);
        EXPECT_EQ(automatic.data_->layout_bindings.front().binding, 0u);
        imported.data_.reset();
        EXPECT_FALSE(observed.expired());
        explicit_binding.data_.reset();
        EXPECT_FALSE(observed.expired());
        automatic.data_.reset();
        EXPECT_TRUE(observed.expired());
    }
    TEST_F(DescriptorVulkanIntegrationTest, ExplicitSentinelsUseTheSameAutomaticBindingAsTheVectorConstructor) {
        using namespace vsl::graphic_resource;
        for (const auto sentinel : {std::size_t{UINT32_MAX}, std::size_t(-1)}) {
            BindingLayout automatic(device, sentinel, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 3);
            EXPECT_EQ(automatic.data_->device, device.data_.get());
            ASSERT_EQ(automatic.data_->layout_bindings.size(), 1u);
            EXPECT_EQ(automatic.data_->layout_bindings[0].binding, 0u);
            EXPECT_EQ(automatic.get_binding_points(),
                      (std::vector<BindingPoint>{{0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 3}}));
        }
    }
} // namespace
#include "support/image_transfer_fixture.hpp"

namespace {
    TEST_P(DescriptorImageBarrierVulkanIntegrationTest, ImageAndCombinedSamplerUpdatesReachComputeShader) {
        using namespace vsl::graphic_resource;
        pipeline.reset();
        layout.reset();
        binding.emplace(device, std::initializer_list<BindingPoint>{
                                    {0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1},
                                    {1, Type::STORAGE_IMAGE, vsl::ShaderFlag::COMPUTE, 1},
                                    SampledTexture::VALUE});
        auto [image_pool, allocated] = resources->make(std::vector{*binding});
        struct ReclaimCycle {
            vsl::GraphicResourcePool pool;
            ~ReclaimCycle() {
                pool.data_->resources.clear();
            }
        } reclaim{image_pool};
        ASSERT_EQ(allocated.size(), 1u);
        resource = allocated.front();
        using StorageImage = vsl::Image<vsl::ImageType::STORAGE | vsl::ImageType::TRANSFER_DST>;
        StorageImage storage(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        SampledImage sampled(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
        vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
        const std::array<std::uint8_t, 4> texel{37, 83, 131, 255};
        vsl::StagingBuffer<vsl::MemoryType::TRANSFER_SOURCE> upload(device, texel);
        shader.emplace(device, vsl_test::shader_path("images.comp.spv"));
        layout.emplace(device, vsl::pipeline_layout::ShaderGroup("images", {*shader}),
                       vsl::pipeline_layout::ResourceBinding{{*binding}});
        pipeline.emplace(*layout);
        resource.update(&*output);
        resource.update(storage, 1);
        resource.update(vsl::ImageAccessor{sampled}, sampler, 2, Type::COMBINED_IMAGE_SAMPLER);
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto change = [&](vsl::ImageAccessor image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                              vk::AccessFlags source, vk::AccessFlags destination) {
                vk::ImageMemoryBarrier barrier{};
                barrier.oldLayout = old_layout;
                barrier.newLayout = new_layout;
                barrier.srcAccessMask = source;
                barrier.dstAccessMask = destination;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image.data_->image;
                barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
                auto buffer = manager.get_buffer();
                CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {},
                    barrier);
            };
            change(storage, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
                   vk::AccessFlagBits::eTransferWrite);
            phase << vsl::command::CopyBufferToImage(storage, &upload, vsl::ImageLayout::GENERAL);
            change(storage, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                   vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
            change(sampled, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                   vk::AccessFlagBits::eTransferWrite);
            phase << vsl::command::CopyBufferToImage(sampled, &upload, vsl::ImageLayout::TRANSFER_DST_OPTIMAL);
            phase << make_shader_read_barrier(GetParam(), sampled);
            phase << *pipeline << vsl::command::BindGraphicResource(resource, BindingDestination::COMPUTE)
                  << vsl::command::Dispatch(1, 1, 1);
            vk::MemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto buffer = manager.get_buffer();
            CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = output->data(2 * sizeof(std::uint32_t));
        std::array<std::uint32_t, 2> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{37, 83}));
    }

    INSTANTIATE_TEST_SUITE_P(ShaderReadStages, DescriptorImageBarrierVulkanIntegrationTest,
                             testing::Values(ImageReadBarrierMode::AUTOMATIC, ImageReadBarrierMode::COMPUTE,
                                             ImageReadBarrierMode::TYPED, ImageReadBarrierMode::MULTI_STAGE,
                                             ImageReadBarrierMode::ALL_GRAPHICS_COMPUTE));
} // namespace

namespace {
    TEST_F(DescriptorVulkanIntegrationTest, HolderValueUpdateUsesItsOffsetAndSizeForTheComputeOutput) {
        const std::size_t offset = std::max<std::size_t>(16, physical.data_->props->limits.minStorageBufferOffsetAlignment);
        std::vector<std::uint32_t> initial(offset / sizeof(std::uint32_t) + COUNT, 0xa5a5a5a5u);
        StorageBuffer buffer(device, initial.data(), initial.size());
        {
            auto mapped = buffer.data(COUNT * sizeof(std::uint32_t), offset);
            resource.update(buffer, std::move(mapped), 0, vsl::graphic_resource::Type::STORAGE_BUFFER);
        }
        std::uint32_t base = 113;
        auto manager = make_manager();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto stream = phase << *pipeline;
            stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                   << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                   << vsl::command::Dispatch(COUNT, 1, 1);
            vk::MemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.get_buffer();
            CommandAccess::Data(commands)->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device.data_->device.waitIdle();
        auto mapped = buffer.data(initial.size() * sizeof(std::uint32_t));
        const auto* actual = static_cast<const std::uint32_t*>(mapped.data);
        for (std::size_t i = 0; i < offset / sizeof(std::uint32_t); ++i)
            EXPECT_EQ(actual[i], 0xa5a5a5a5u);
        for (std::size_t i = 0; i < COUNT; ++i)
            EXPECT_EQ(actual[offset / sizeof(std::uint32_t) + i], base + i);
    }
} // namespace

namespace {
    TEST_F(DescriptorVulkanIntegrationTest, BufferUpdatesRejectEmptyAndOutOfBoundsRangesBeforeCallingVulkan) {
        const auto bytes = COUNT * sizeof(std::uint32_t);
        EXPECT_THROW(resource.update(&*output, 0, std::nullopt, bytes), vsl::exceptions::RuntimeException);
        EXPECT_THROW(resource.update(&*output, 0, std::nullopt, bytes + 1), vsl::exceptions::RuntimeException);
        EXPECT_THROW(resource.update(&*output, 0, std::nullopt, 0, 0), vsl::exceptions::RuntimeException);
        EXPECT_THROW(resource.update(&*output, 0, std::nullopt, 0, bytes + 1), vsl::exceptions::RuntimeException);
    }
    TEST_F(DescriptorVulkanIntegrationTest, DefaultBufferRangeAtAnOffsetDoesNotExceedTheRequestedStorage) {
        const auto offset = std::max<std::size_t>(16, physical.data_->props->limits.minStorageBufferOffsetAlignment);
        StorageBuffer buffer(device, std::size_t{offset + COUNT * sizeof(std::uint32_t)});
        resource.update(&buffer, 0, std::nullopt, offset);
    }
    TEST_F(DescriptorVulkanIntegrationTest, DefaultExplicitAndWholeBufferRangesWriteOnlyTheSelectedTailOnGPU) {
        const auto offset = std::max<std::size_t>(16, physical.data_->props->limits.minStorageBufferOffsetAlignment);
        const std::vector<std::uint32_t> initial(offset / sizeof(std::uint32_t) + COUNT, 0xa5a5a5a5u);
        StorageBuffer buffer(device, initial.data(), initial.size());
        std::vector<std::optional<std::size_t>> ranges{std::nullopt, COUNT * sizeof(std::uint32_t)};
        if constexpr (sizeof(std::size_t) >= sizeof(vk::DeviceSize))
            ranges.emplace_back(static_cast<std::size_t>(VK_WHOLE_SIZE));
        for (auto range : ranges) {
            ASSERT_TRUE(buffer.copy(initial.data(), initial.size()));
            resource.update(&buffer, 0, std::nullopt, offset, range);
            const std::uint32_t base = 137;
            auto manager = make_manager();
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                auto stream = phase << *pipeline;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                       << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                       << vsl::command::Dispatch(COUNT, 1, 1);
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto commands = manager.get_buffer();
                CommandAccess::Data(commands)->command_buffers[commands.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device.data_->device.waitIdle();
            auto mapped = buffer.data(initial.size() * sizeof(std::uint32_t));
            const auto* actual = static_cast<const std::uint32_t*>(mapped.data);
            for (std::size_t i = 0; i < offset / sizeof(std::uint32_t); ++i)
                EXPECT_EQ(actual[i], initial[i]);
            for (std::size_t i = 0; i < COUNT; ++i)
                EXPECT_EQ(actual[offset / sizeof(std::uint32_t) + i], base + i);
        }
    }
} // namespace

namespace {
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, PoolAndResourceRemainUsableAfterManagerMoveAndDestruction) {
        using namespace vsl::graphic_resource;
        BindingLayout layout(device, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
        vsl::StagingBuffer<vsl::MemoryType::STORAGE_BUFFER> buffer(device, 16);
        Pool pool;
        Resource retained;
        {
            vsl::GraphicResourceManager manager(device);
            auto [created, allocated] = manager.make(std::vector{layout}, std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, 3}});
            pool = created;
            retained = allocated.front();
            auto moved = std::move(manager);
            EXPECT_EQ(pool.get_manager(), &moved);
            retained.update(&buffer);
            auto [success, extra] = pool.bind(layout);
            ASSERT_TRUE(success);
            extra.update(&buffer);
        }
        EXPECT_EQ(pool.get_manager(), nullptr);
        EXPECT_NO_THROW(retained.update(&buffer));
        auto [success, extra] = pool.bind(2, layout);
        ASSERT_TRUE(success);
        ASSERT_EQ(extra.size(), 2u);
        EXPECT_NO_THROW(extra.front().update(&buffer));
    }
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, ManagerCopyAndAssignmentDoNotLeaveDanglingPoolObservers) {
        using namespace vsl::graphic_resource;
        vsl::GraphicResourceManager manager(device);
        auto pool = manager.make(std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, 1}});
        auto copied = manager;
        BindingLayout layout(device, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
        auto [success, resource] = pool.bind(layout);
        ASSERT_TRUE(success);
        manager = vsl::GraphicResourceManager(device);
        EXPECT_EQ(pool.get_manager(), nullptr);
        vsl::StagingBuffer<vsl::MemoryType::STORAGE_BUFFER> buffer(device, 16);
        EXPECT_NO_THROW(resource.update(&buffer));
        copied.destroy(pool);
        EXPECT_THROW(resource.update(&buffer), std::logic_error);
        EXPECT_THROW(pool.bind(layout), std::logic_error);
        EXPECT_THROW(pool.bind(1, layout), std::logic_error);
        EXPECT_THROW(pool.bind(std::vector{layout}), std::logic_error);
    }
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, UpdatedBuffersAreOwnedPerBindingAndReplacementReleasesTheOldBuffer) {
        using namespace vsl::graphic_resource;
        vsl::GraphicResourceManager manager(device);
        BindingLayout layout(device, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
        auto [pool, allocated] = manager.make(std::vector{layout});
        auto resource = allocated.front();
        std::weak_ptr<vsl::impl::BufferImpl> weak;
        {
            vsl::StagingBuffer<vsl::MemoryType::STORAGE_BUFFER> buffer(device, 16);
            weak = buffer.data_;
            resource.update(&buffer);
        }
        EXPECT_FALSE(weak.expired());
        vsl::StagingBuffer<vsl::MemoryType::STORAGE_BUFFER> replacement(device, 16);
        resource.update(&replacement);
        EXPECT_TRUE(weak.expired());
    }
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, DroppingTheLastResourceAliasReleasesItsNativeSetAndOwners) {
        using namespace vsl::graphic_resource;
        vsl::GraphicResourceManager manager(device);
        BindingLayout layout(device, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE);
        auto pool = manager.make(std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, 1}});
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak;
        {
            auto [success, resource] = pool.bind(layout);
            ASSERT_TRUE(success);
            weak = resource.data_;
        }
        EXPECT_TRUE(weak.expired());
        auto [success, replacement] = pool.bind(layout);
        EXPECT_TRUE(success);
        EXPECT_EQ(pool.data_->resources.size(), 1u);
        auto [empty_ok, empty] = pool.bind(0, layout);
        EXPECT_TRUE(empty_ok);
        EXPECT_TRUE(empty.empty());
        auto [vector_ok, vector] = pool.bind(std::vector<BindingLayout>{});
        EXPECT_TRUE(vector_ok);
        EXPECT_TRUE(vector.empty());
    }
} // namespace

namespace {
    TEST_F(DescriptorOwnershipVulkanIntegrationTest, ImageAndSamplerOwnersSurviveTheirWrappers) {
        using namespace vsl::graphic_resource;
        vsl::GraphicResourceManager manager(device);
        BindingLayout layout(device, {{0, Type::STORAGE_IMAGE, vsl::ShaderFlag::COMPUTE, 1},
                                      {1, Type::COMBINED_IMAGE_SAMPLER, vsl::ShaderFlag::COMPUTE, 1}});
        auto [pool, allocated] = manager.make(std::vector{layout});
        std::weak_ptr<vsl::impl::ImageImpl> weak_image;
        std::weak_ptr<vsl::impl::SamplerImpl> weak_sampler;
        {
            vsl::Image<vsl::ImageType::STORAGE | vsl::ImageType::SAMPLED> image(device, 1, 1, vsl::data_format::UNSIGNED_NORMALIZED8_RGBA);
            vsl::Sampler sampler(device, vsl::SamplingMode::NEAREST, vsl::EdgeMode::CLAMP_TO_EDGE);
            weak_image = image.data_;
            weak_sampler = sampler.data_;
            allocated.front().update(image, 0);
            allocated.front().update(image, sampler, 1);
        }
        EXPECT_FALSE(weak_image.expired());
        EXPECT_FALSE(weak_sampler.expired());
        allocated.clear();
        EXPECT_TRUE(weak_image.expired());
        EXPECT_TRUE(weak_sampler.expired());
    }
} // namespace

namespace {
    TEST_F(DescriptorVulkanIntegrationTest, MissingBufferBindingReportsItsCauseAndKeepsValidBindingsUsable) {
        bool rejected = false;
        try {
            resource.update(&*output, 99);
        } catch (const vsl::exceptions::RuntimeException& error) {
            rejected = true;
            EXPECT_NE(error.reason.find("binding"), std::string::npos) << error.reason;
        }
        ASSERT_TRUE(rejected) << "An unknown binding must not reach Vulkan with an unset descriptor type";
        resource.update(&*output);
        ExpectSequence(execute(149), 149);
    }
    TEST_F(DescriptorVulkanIntegrationTest, ExplicitBufferTypeWorksWithoutLayoutMetadataWhileInferenceReportsTheMissingLayout) {
        resource.data_->layout = nullptr;
        try {
            resource.update(&*output);
            FAIL() << "Inferring the descriptor type requires layout metadata";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(error.reason.find("Layout"), std::string::npos);
        }
        EXPECT_NO_THROW(resource.update(&*output, 0, vsl::graphic_resource::Type::STORAGE_BUFFER));
        resource = {};
        EXPECT_EQ(pool.data_->allocated_sets, 0u);
        EXPECT_EQ(pool.data_->capacities.at(vk::DescriptorType::eStorageBuffer).allocated, 0u);
        auto [success, allocated] = pool.bind(4, *binding);
        ASSERT_TRUE(success);
        ASSERT_EQ(allocated.size(), 4u);
        resource = allocated.front();
        resource.update(&*output);
        ExpectSequence(execute(151), 151);
    }
    TEST_F(DescriptorVulkanIntegrationTest, DescriptorTypeCapacityRejectsOverflowWithoutChangingLiveSetsAndRecoversAfterFree) {
        auto [success, extra] = pool.bind(3, *binding);
        ASSERT_TRUE(success);
        ASSERT_EQ(extra.size(), 3u);
        EXPECT_EQ(pool.data_->allocated_sets, 4u);
        const auto before = pool.data_->resources.size();
        auto [overflow, rejected] = pool.bind(*binding);
        EXPECT_FALSE(overflow);
        EXPECT_FALSE(rejected.data_);
        EXPECT_EQ(pool.data_->resources.size(), before);
        EXPECT_EQ(pool.data_->allocated_sets, 4u);
        ExpectSequence(execute(157), 157);
        extra.clear();
        EXPECT_EQ(pool.data_->allocated_sets, 1u);
        auto [retried, replacement] = pool.bind(3, *binding);
        EXPECT_TRUE(retried);
        EXPECT_EQ(replacement.size(), 3u);
    }
    TEST_F(DescriptorVulkanIntegrationTest, ReservedZeroCountBindingsDoNotConsumeDescriptorCapacity) {
        using namespace vsl::graphic_resource;
        BindingLayout reserved(device, {{7, Type::STORAGE_BUFFER, static_cast<vsl::ShaderFlag>(0), 0}});
        auto [empty_pool, allocated] = resources->make(std::vector{reserved});
        ASSERT_EQ(allocated.size(), 1u);
        EXPECT_TRUE(empty_pool.data_->capacities.empty());
        EXPECT_EQ(empty_pool.data_->allocated_sets, 1u);
        EXPECT_NO_THROW(vsl::typed::detail::require_descriptor_complete(allocated.front()));
        auto group = vsl::ResourceUseGroup::ForDescriptor(1, allocated.front());
        EXPECT_NO_THROW(group->replace(0, {}));
        EXPECT_THROW(allocated.front().update(&*output, 7), std::out_of_range);
        group.reset();
        allocated.clear();
        EXPECT_EQ(empty_pool.data_->allocated_sets, 0u);
    }
} // namespace
