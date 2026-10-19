#include "support/compute_fixture.hpp"
namespace {
    using PushConstantVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(PushConstantVulkanIntegrationTest, TypedAndExplicitSizeOverloadsReachShader) {
        std::uint32_t base = 37;
        vsl::command::PushConstant command(*layout, vsl::ShaderFlag::Compute, base);
        EXPECT_EQ(command.layout._data, layout->_data);
        EXPECT_EQ(command.value, &base);
        EXPECT_EQ(command.size, 4u);
        EXPECT_EQ(command.offset, 0u);
        EXPECT_EQ(command.targetType, vsl::ShaderFlag::Compute);
        expect_sequence(execute(base), base);
        expect_sequence(execute(73, false, true), 73);
    }
} // namespace

namespace {
    TEST_F(PushConstantVulkanIntegrationTest, ConstTypedAndExplicitValuesReachTheShader) {
        for (const bool explicit_size : {false, true}) {
            const std::uint32_t base = explicit_size ? 197 : 149;
            auto command = explicit_size
                               ? vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, sizeof(base), 0, &base)
                               : vsl::command::PushConstant(*layout, vsl::ShaderFlag::Compute, base);
            EXPECT_EQ(command.value, &base);
            EXPECT_EQ(command.size, sizeof(base));
            auto manager = make_manager();
            {
                auto phase = manager.startPhase<vsl::ComputePhase>();
                auto stream = phase << *pipeline;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::Compute)
                       << command << vsl::command::Dispatch(count, 1, 1);
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto buffer = manager.getBuffer();
                buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device._data->device.waitIdle();
            auto mapped = output->data(count * sizeof(std::uint32_t));
            std::array<std::uint32_t, count> actual{};
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            expect_sequence(actual, base);
        }
    }
} // namespace

namespace {
    TEST_F(PushConstantVulkanIntegrationTest, SharedVertexAndComputeOffsetsReachEveryComputeOutput) {
        namespace pl = vsl::pipeline_layout;
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::Vertex, 0, 4}, {vsl::ShaderFlag::Compute, 0, 4}};
        vsl::PipelineLayout edited(device, pl::ShaderGroup("shared-push", {*shader}),
                                   pl::ResourceBinding{{*binding}}, constants);
        const auto& info = edited._data->info->pipelineLayout;
        bool found = false;
        for (unsigned i = 0; i < info.pushConstantRangeCount; ++i)
            if (info.pPushConstantRanges[i].stageFlags & vk::ShaderStageFlagBits::eCompute) {
                ASSERT_EQ(info.pPushConstantRanges[i].offset, 0u); // 不正な範囲をshaderへ渡す前に検出する。
                found = true;
            }
        ASSERT_TRUE(found);
        auto frozen = std::move(edited).freeze();
        vsl::ComputePipeline sharedPipeline(frozen);
        auto manager = make_manager();
        std::uint32_t base = 311;
        {
            vsl::ComputePhase phase(manager);
            auto stream = phase << sharedPipeline;
            stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::Compute)
                   << vsl::command::PushConstant(frozen, vsl::ShaderFlag::Vertex | vsl::ShaderFlag::Compute, base)
                   << vsl::command::Dispatch(count);
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto buffer = manager.getBuffer();
            buffer._data->commandBuffers[buffer.getCurrentBufferIdx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        manager.waitIdle();
        const auto mapped = output->data(count * sizeof(std::uint32_t));
        std::array<std::uint32_t, count> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        expect_sequence(actual, base);
    }
} // namespace
