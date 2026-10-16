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
TEST_F(DescriptorVulkanIntegrationTest, DISABLED_SingleBindingConstructorsRetainDeviceOwner) {
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
}
} // namespace
#include "support/compile_probe.hpp"
namespace {
TEST(ResourceCompilationTest, DISABLED_ImageValueUpdateResolvesToImageAccessor) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(vsl::GraphicResource resource,vsl::Image<vsl::ImageType::Sampled> image){resource.update(image);}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
}
TEST(ResourceCompilationTest, DISABLED_HolderUpdateResolvesToBufferRange) {
    const auto result = vsl_test::compile_probe(R"cpp(
#include <VSL/vulkan/_pimpls.hpp>
#include <VSL/vsl.hpp>
void probe(vsl::GraphicResource resource,vsl::StagingBuffer<vsl::MemoryType::UniformBuffer>& buffer){
    resource.update(buffer,buffer.data());
}
)cpp");
    EXPECT_EQ(result.status, 0) << result.diagnostics;
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
    resource.update(vsl::ImageAccessor{storage}, 1);
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
