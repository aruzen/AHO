#pragma once
#include "vulkan_fixture.hpp"
#include <VSL/vulkan/commands/dispatch.hpp>
#include <VSL/vulkan/commands/push_content.hpp>
#include <optional>

namespace vsl_test {
class ComputeVulkanIntegrationTest : public VulkanIntegrationTest {
  protected:
    static constexpr std::size_t count = 16;
    using StorageBuffer = vsl::StagingBuffer<vsl::MemoryType::StorageBuffer>;
    std::optional<StorageBuffer> output;
    std::optional<vsl::graphic_resource::BindingLayout> binding;
    std::optional<vsl::GraphicResourceManager> resources;
    vsl::GraphicResourcePool pool;
    vsl::GraphicResource resource;
    std::optional<vsl::Shader<vsl::ShaderType::Compute>> shader;
    std::optional<vsl::PipelineLayout> layout;
    std::optional<vsl::ComputePipeline> pipeline;
    void SetUp() override {
        VulkanIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        if (!host_coherent_available())
            GTEST_SKIP() << "Host coherent memory unavailable";
        output.emplace(device, count * sizeof(std::uint32_t));
        std::array<std::uint32_t, count> zero{};
        ASSERT_TRUE(output->copy(zero));
        using namespace vsl::graphic_resource;
        binding.emplace(device, std::initializer_list<BindingPoint>{
                                    {0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 1}});
        resources.emplace(device);
        pool = resources->make(std::map<Type, std::size_t>{{Type::StorageBuffer, 4}});
        auto [success, allocated] = pool.bind(std::vector<BindingLayout>{*binding});
        ASSERT_TRUE(success);
        ASSERT_EQ(allocated.size(), 1u);
        resource = allocated.front();
        resource.update(&*output);
        shader.emplace(device, shader_path("fill.comp.spv"));
        namespace pl = vsl::pipeline_layout;
        layout.emplace(device, pl::ShaderGroup("compute", {*shader}), pl::ResourceBinding{{*binding}},
                       pl::PushConstants(vsl::ShaderFlag::Compute, {{vsl::ShaderFlag::Compute, 0, 4}}));
        pipeline.emplace(*layout);
    }
    std::array<std::uint32_t, count> execute(std::uint32_t base, bool explicit_binding = false,
                                             bool explicit_push = false) {
        auto manager = make_manager();
        {
            auto phase = manager.startPhase<vsl::ComputePhase>();
            auto stream = phase << *pipeline;
            if (explicit_binding)
                stream << vsl::command::BindGraphicResource(
                    std::vector<vsl::GraphicResource>{resource},
                    vsl::graphic_resource::BindingDestination::Compute, *pipeline);
            else
                stream << vsl::command::BindGraphicResource(
                    resource, vsl::graphic_resource::BindingDestination::Compute);
            if (explicit_push)
                stream << vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, sizeof(base), 0,
                                                     &base);
            else
                stream << vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, base);
            stream << vsl::command::Dispatch(count, 1, 1);
            // Make shader writes visible to the host before observing the payload.
            vk::MemoryBarrier barrier{};
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
            auto buffer = manager.getBuffer();
            buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {},
                {});
        }
        device._data->device.waitIdle();
        auto mapped = output->data(count * sizeof(std::uint32_t));
        std::array<std::uint32_t, count> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        return actual;
    }
    static void expect_sequence(const std::array<std::uint32_t, count> &actual, std::uint32_t base) {
        for (std::size_t i = 0; i < count; ++i)
            EXPECT_EQ(actual[i], base + i) << "index=" << i;
    }
    void TearDown() override {
        if (device._data)
            device._data->device.waitIdle();
        pipeline.reset();
        layout.reset();
        shader.reset();
        resource._data.reset();
        // Pool and Resource hold each other strongly (I23). Break fixture-owned cycles.
        if (resources)
            for (auto &owned_pool : resources->_data->pools)
                owned_pool->resources.clear();
        pool._data.reset();
        resources.reset();
        binding.reset();
        output.reset();
        VulkanIntegrationTest::TearDown();
    }
};
} // namespace vsl_test
