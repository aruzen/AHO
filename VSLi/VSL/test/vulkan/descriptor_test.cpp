#include "support/compute_fixture.hpp"
namespace {
using DescriptorVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
TEST(BindingPointTest, ComparisonIncludesBindingTypeStageAndAmount) {
    using namespace vsl::graphic_resource;
    BindingPoint first{2, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1};
    EXPECT_EQ(first, (BindingPoint{2, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}));
    EXPECT_NE(first, (BindingPoint{3, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}));
    EXPECT_EQ(static_cast<unsigned>(Type::StorageBuffer),
              static_cast<unsigned>(vk::DescriptorType::eStorageBuffer));
    EXPECT_EQ(static_cast<unsigned>(BindingDestination::Compute),
              static_cast<unsigned>(vk::PipelineBindPoint::eCompute));
}
TEST_F(DescriptorVulkanIntegrationTest, VectorAndInitializerLayoutsPreservePointsAndAutomaticNumbers) {
    using namespace vsl::graphic_resource;
    const std::vector<BindingPoint> points{{7, Type::UniformBuffer, vsl::ShaderFlag::Vertex, 2},
                                           {UINT32_MAX, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}};
    BindingLayout layout(device, points);
    auto expected = points;
    expected[1].binding = 1;
    EXPECT_EQ(layout.getBindingPoints(), expected);
    EXPECT_EQ(binding->getBindingPoints(),
              (std::vector<BindingPoint>{{0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}}));
}
TEST_F(DescriptorVulkanIntegrationTest, ResourcePoolAndManagerIdentityArePreserved) {
    EXPECT_EQ(pool.getManager(), &*resources);
    EXPECT_EQ(resource.getPool()._data, pool._data);
    expect_sequence(execute(17), 17);
}
TEST_F(DescriptorVulkanIntegrationTest, BindsSingleAndRepeatedLayoutsWithExplicitTypeUpdates) {
    using namespace vsl::graphic_resource;
    auto [single_ok, single] = pool.bind(*binding);
    ASSERT_TRUE(single_ok);
    ASSERT_TRUE(single._data);
    single.update(&*output, 0, Type::StorageBuffer, 0, count * sizeof(std::uint32_t));
    auto [many_ok, many] = pool.bind(2, *binding);
    ASSERT_TRUE(many_ok);
    ASSERT_EQ(many.size(), 2u);
    EXPECT_NE(many[0]._data->descriptorSet, many[1]._data->descriptorSet);
    EXPECT_EQ(many[0].getPool()._data, pool._data);
    // These overloads omit the layout owner (I13); explicit type remains usable.
    EXPECT_THROW(single.update(&*output), vsl::exceptions::RuntimeException);
    resource = single;
    expect_sequence(execute(23), 23);
}
TEST_F(DescriptorVulkanIntegrationTest, ValueUpdatePreservesBufferDataAndRange) {
    resource.update(*output, 0, vsl::graphic_resource::Type::StorageBuffer, 0, count * sizeof(std::uint32_t));
    expect_sequence(execute(31), 31);
}
TEST_F(DescriptorVulkanIntegrationTest, SingleBindingConstructorsRetainDeviceOwner) {
    using namespace vsl::graphic_resource;
    BindingLayout explicit_binding(device, 2, Type::UniformBuffer, vsl::ShaderFlag::Vertex, 1);
    const bool had_owner = static_cast<bool>(explicit_binding._data->device);
    explicit_binding._data->device = device._data; // Ensure safe test-only destruction (I13).
    EXPECT_TRUE(had_owner);
    EXPECT_EQ(explicit_binding.getBindingPoints(),
              (std::vector<BindingPoint>{{2, Type::UniformBuffer, vsl::ShaderFlag::Vertex, 1}}));
    BindingLayout automatic(device, Type::UniformBuffer, vsl::ShaderFlag::Vertex);
    const bool automatic_owner = static_cast<bool>(automatic._data->device);
    automatic._data->device = device._data;
    EXPECT_TRUE(automatic_owner);
    EXPECT_EQ(automatic.getBindingPoints().front().binding, 0u);
    EXPECT_EQ(automatic._data->layoutBindings.front().binding, 0u);
}
TEST_F(DescriptorVulkanIntegrationTest, ExplicitSentinelsUseTheSameAutomaticBindingAsTheVectorConstructor) {
    using namespace vsl::graphic_resource;
    for (const auto sentinel : {std::size_t{UINT32_MAX}, std::size_t(-1)}) {
        BindingLayout automatic(device, sentinel, Type::StorageBuffer, vsl::ShaderFlag::Compute, 3);
        const auto original_owner = automatic._data->device;
        if (!original_owner) automatic._data->device = device._data;
        EXPECT_EQ(original_owner, device._data);
        ASSERT_EQ(automatic._data->layoutBindings.size(), 1u);
        EXPECT_EQ(automatic._data->layoutBindings[0].binding, 0u);
        EXPECT_EQ(automatic.getBindingPoints(),
                  (std::vector<BindingPoint>{{0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 3}}));
    }
}
} // namespace
#include "support/image_transfer_fixture.hpp"
namespace {
TEST_F(DescriptorVulkanIntegrationTest, ImageAndCombinedSamplerUpdatesReachComputeShader) {
    using namespace vsl::graphic_resource;
    pipeline.reset();
    layout.reset();
    binding.emplace(device, std::initializer_list<BindingPoint>{
                                {0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1},
                                {1, Type::StorageImage, vsl::ShaderFlag::Compute, 1},
                                {2, Type::CombinedImageSampler, vsl::ShaderFlag::Compute, 1}});
    auto [image_pool, allocated] = resources->make(std::vector{*binding});
    struct ReclaimCycle {
        vsl::GraphicResourcePool pool;
        ~ReclaimCycle() {
            pool._data->resources.clear();
        }
    } reclaim{image_pool};
    ASSERT_EQ(allocated.size(), 1u);
    resource = allocated.front();
    using StorageImage = vsl::Image<vsl::ImageType::Storage | vsl::ImageType::TransferDst>;
    using SampledImage = vsl::Image<vsl::ImageType::Sampled | vsl::ImageType::TransferDst>;
    StorageImage storage(device, 1, 1, vsl::data_format::UnsignedNormalized8RGBA);
    SampledImage sampled(device, 1, 1, vsl::data_format::UnsignedNormalized8RGBA);
    vsl::Sampler sampler(device, vsl::SamplingMode::Nearest, vsl::EdgeMode::ClampToEdge);
    const std::array<std::uint8_t, 4> texel{37, 83, 131, 255};
    vsl::StagingBuffer<vsl::MemoryType::TransferSource> upload(device, texel);
    shader.emplace(device, vsl_test::shader_path("images.comp.spv"));
    layout.emplace(device, vsl::pipeline_layout::ShaderGroup("images", {*shader}),
                   vsl::pipeline_layout::ResourceBinding{{*binding}});
    pipeline.emplace(*layout);
    resource.update(&*output);
    resource.update(storage, 1);
    resource.update(vsl::ImageAccessor{sampled}, sampler, 2, Type::CombinedImageSampler);
    auto manager = make_manager();
    {
        auto phase = manager.startPhase<vsl::ComputePhase>();
        auto change = [&](vsl::ImageAccessor image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                          vk::AccessFlags source, vk::AccessFlags destination) {
            vk::ImageMemoryBarrier barrier{};
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcAccessMask = source;
            barrier.dstAccessMask = destination;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image._data->image;
            barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
            auto buffer = manager.getBuffer();
            buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {},
                barrier);
        };
        change(storage, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
               vk::AccessFlagBits::eTransferWrite);
        phase << vsl::command::CopyBufferToImage(storage, &upload, vsl::ImageLayout::General);
        change(storage, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
               vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
        change(sampled, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
               vk::AccessFlagBits::eTransferWrite);
        phase << vsl::command::CopyBufferToImage(sampled, &upload, vsl::ImageLayout::TransferDstOptimal);
        phase << vsl::command::ChangeImageBarrier(sampled, vsl::ImageLayout::TransferDstOptimal,
                                                  vsl::ImageLayout::ShaderReadOnlyOptimal);
        // The public barrier targets FragmentShader only (I10). Extend visibility
        // in the fixture so this descriptor test has a valid ComputeShader dependency.
        change(sampled, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
               vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
        phase << *pipeline << vsl::command::BindGraphicResource(resource, BindingDestination::Compute)
              << vsl::command::Dispatch(1, 1, 1);
        vk::MemoryBarrier barrier{};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
        auto buffer = manager.getBuffer();
        buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
    }
    device._data->device.waitIdle();
    auto mapped = output->data(2 * sizeof(std::uint32_t));
    std::array<std::uint32_t, 2> actual{};
    std::memcpy(actual.data(), mapped.data, sizeof(actual));
    EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{37, 83}));
}
} // namespace

namespace {
TEST_F(DescriptorVulkanIntegrationTest, HolderValueUpdateUsesItsOffsetAndSizeForTheComputeOutput) {
    const std::size_t offset = std::max<std::size_t>(16, physical._data->props->limits.minStorageBufferOffsetAlignment);
    std::vector<std::uint32_t> initial(offset / sizeof(std::uint32_t) + count, 0xa5a5a5a5u);
    StorageBuffer buffer(device, initial.data(), initial.size());
    {
        auto mapped = buffer.data(count * sizeof(std::uint32_t), offset);
        resource.update(buffer, std::move(mapped), 0, vsl::graphic_resource::Type::StorageBuffer);
    }
    std::uint32_t base = 113;
    auto manager = make_manager();
    {
        auto phase = manager.startPhase<vsl::ComputePhase>();
        auto stream = phase << *pipeline;
        stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::Compute)
               << vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, base)
               << vsl::command::Dispatch(count, 1, 1);
        vk::MemoryBarrier barrier{};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
        auto commands = manager.getBuffer();
        commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
    }
    device._data->device.waitIdle();
    auto mapped = buffer.data(initial.size() * sizeof(std::uint32_t));
    const auto *actual = static_cast<const std::uint32_t *>(mapped.data);
    for (std::size_t i = 0; i < offset / sizeof(std::uint32_t); ++i) EXPECT_EQ(actual[i], 0xa5a5a5a5u);
    for (std::size_t i = 0; i < count; ++i) EXPECT_EQ(actual[offset / sizeof(std::uint32_t) + i], base + i);
}
} // namespace

namespace {
TEST_F(DescriptorVulkanIntegrationTest, BufferUpdatesRejectEmptyAndOutOfBoundsRangesBeforeCallingVulkan) {
    const auto bytes = count * sizeof(std::uint32_t);
    EXPECT_THROW(resource.update(&*output, 0, std::nullopt, bytes), vsl::exceptions::RuntimeException);
    EXPECT_THROW(resource.update(&*output, 0, std::nullopt, bytes + 1), vsl::exceptions::RuntimeException);
    EXPECT_THROW(resource.update(&*output, 0, std::nullopt, 0, 0), vsl::exceptions::RuntimeException);
    EXPECT_THROW(resource.update(&*output, 0, std::nullopt, 0, bytes + 1), vsl::exceptions::RuntimeException);
}
TEST_F(DescriptorVulkanIntegrationTest, DefaultBufferRangeAtAnOffsetDoesNotExceedTheRequestedStorage) {
    const auto offset = std::max<std::size_t>(16, physical._data->props->limits.minStorageBufferOffsetAlignment);
    StorageBuffer buffer(device, std::size_t{offset + count * sizeof(std::uint32_t)});
    resource.update(&buffer, 0, std::nullopt, offset);
}
TEST_F(DescriptorVulkanIntegrationTest, DefaultExplicitAndWholeBufferRangesWriteOnlyTheSelectedTailOnGpu) {
    const auto offset = std::max<std::size_t>(16, physical._data->props->limits.minStorageBufferOffsetAlignment);
    const std::vector<std::uint32_t> initial(offset / sizeof(std::uint32_t) + count, 0xa5a5a5a5u);
    StorageBuffer buffer(device, initial.data(), initial.size());
    std::vector<std::optional<std::size_t>> ranges{std::nullopt, count * sizeof(std::uint32_t)};
    if constexpr (sizeof(std::size_t) >= sizeof(vk::DeviceSize))
        ranges.emplace_back(static_cast<std::size_t>(VK_WHOLE_SIZE));
    for (auto range : ranges) {
        ASSERT_TRUE(buffer.copy(initial.data(), initial.size()));
        resource.update(&buffer, 0, std::nullopt, offset, range);
        const std::uint32_t base = 137;
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            auto stream = phase << *pipeline;
            stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::Compute)
                   << vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, base)
                   << vsl::command::Dispatch(count, 1, 1);
            vk::MemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto commands = manager.getBuffer();
            commands._data->commandBuffers[commands.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        device._data->device.waitIdle();
        auto mapped = buffer.data(initial.size() * sizeof(std::uint32_t));
        const auto *actual = static_cast<const std::uint32_t *>(mapped.data);
        for (std::size_t i = 0; i < offset / sizeof(std::uint32_t); ++i) EXPECT_EQ(actual[i], initial[i]);
        for (std::size_t i = 0; i < count; ++i) EXPECT_EQ(actual[offset / sizeof(std::uint32_t) + i], base + i);
    }
}
} // namespace

namespace {
TEST_F(DescriptorVulkanIntegrationTest, MissingBufferBindingReportsItsCauseAndKeepsValidBindingsUsable) {
    bool rejected = false;
    try {
        resource.update(&*output, 99);
    } catch (const vsl::exceptions::RuntimeException &error) {
        rejected = true;
        EXPECT_NE(error.reason.find("binding"), std::string::npos) << error.reason;
    }
    ASSERT_TRUE(rejected) << "An unknown binding must not reach Vulkan with an unset descriptor type";
    resource.update(&*output);
    expect_sequence(execute(149), 149);
}
TEST_F(DescriptorVulkanIntegrationTest, ExplicitBufferTypeWorksWithoutLayoutMetadataWhileInferenceReportsTheMissingLayout) {
    resource._data->layout.reset();
    try {
        resource.update(&*output);
        FAIL() << "Inferring the descriptor type requires layout metadata";
    } catch (const vsl::exceptions::RuntimeException &error) {
        EXPECT_NE(error.reason.find("Layout"), std::string::npos);
    }
    EXPECT_NO_THROW(resource.update(&*output, 0, vsl::graphic_resource::Type::StorageBuffer));
}
} // namespace
