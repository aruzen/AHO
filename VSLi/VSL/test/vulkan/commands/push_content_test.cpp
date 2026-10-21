#include "support/compute_fixture.hpp"
namespace {
    using PushConstantVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(PushConstantVulkanIntegrationTest, TypedAndExplicitSizeOverloadsReachShader) {
        std::uint32_t base = 37;
        vsl::command::PushConstant command(*layout, vsl::ShaderFlag::COMPUTE, base);
        EXPECT_EQ(command.layout.data_, layout->data_);
        EXPECT_EQ(command.value, &base);
        EXPECT_EQ(command.size, 4u);
        EXPECT_EQ(command.offset, 0u);
        EXPECT_EQ(command.target_type, vsl::ShaderFlag::COMPUTE);
        ExpectSequence(execute(base), base);
        ExpectSequence(execute(73, false, true), 73);
    }
} // namespace

namespace {
    TEST_F(PushConstantVulkanIntegrationTest, ConstTypedAndExplicitValuesReachTheShader) {
        for (const bool explicit_size : {false, true}) {
            const std::uint32_t base = explicit_size ? 197 : 149;
            auto command = explicit_size
                               ? vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, sizeof(base), 0, &base)
                               : vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base);
            EXPECT_EQ(command.value, &base);
            EXPECT_EQ(command.size, sizeof(base));
            auto manager = make_manager();
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                auto stream = phase << *pipeline;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                       << command << vsl::command::Dispatch(COUNT, 1, 1);
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto buffer = manager.get_buffer();
                buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            }
            device.data_->device.waitIdle();
            auto mapped = output->data(COUNT * sizeof(std::uint32_t));
            std::array<std::uint32_t, COUNT> actual{};
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            ExpectSequence(actual, base);
        }
    }
} // namespace

namespace {
    TEST_F(PushConstantVulkanIntegrationTest, SharedVertexAndComputeOffsetsReachEveryComputeOutput) {
        namespace pl = vsl::pipeline_layout;
        pl::PushConstants constants;
        constants.definitions = {{vsl::ShaderFlag::VERTEX, 0, 4}, {vsl::ShaderFlag::COMPUTE, 0, 4}};
        vsl::PipelineLayout edited(device, pl::ShaderGroup("shared-push", {*shader}),
                                   pl::ResourceBinding{{*binding}}, constants);
        const auto& info = edited.data_->info->pipeline_layout;
        bool found = false;
        for (unsigned i = 0; i < info.pushConstantRangeCount; ++i)
            if (info.pPushConstantRanges[i].stageFlags & vk::ShaderStageFlagBits::eCompute) {
                ASSERT_EQ(info.pPushConstantRanges[i].offset, 0u); // 不正な範囲をshaderへ渡す前に検出する。
                found = true;
            }
        ASSERT_TRUE(found);
        auto frozen = std::move(edited).freeze();
        vsl::ComputePipeline shared_pipeline(frozen);
        auto manager = make_manager();
        std::uint32_t base = 311;
        {
            vsl::ComputePhase phase(manager);
            auto stream = phase << shared_pipeline;
            stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                   << vsl::command::PushConstant(frozen, vsl::ShaderFlag::VERTEX | vsl::ShaderFlag::COMPUTE, base)
                   << vsl::command::Dispatch(COUNT);
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
            auto buffer = manager.get_buffer();
            buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
        }
        manager.wait_idle();
        const auto mapped = output->data(COUNT * sizeof(std::uint32_t));
        std::array<std::uint32_t, COUNT> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        ExpectSequence(actual, base);
    }
} // namespace
