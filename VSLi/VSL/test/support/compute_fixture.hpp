#pragma once
#include "vulkan_fixture.hpp"
#include <VSL/vulkan/commands/dispatch.hpp>
#include <VSL/vulkan/commands/push_content.hpp>
#include <optional>

namespace vsl_test {
    class ComputeVulkanIntegrationTest : public VulkanIntegrationTest {
    protected:
        static constexpr std::size_t COUNT = 16;
        using StorageBuffer = vsl::StagingBuffer<vsl::MemoryType::STORAGE_BUFFER>;
        std::optional<StorageBuffer> output;
        std::optional<vsl::graphic_resource::BindingLayout> binding;
        std::optional<vsl::GraphicResourceManager> resources;
        vsl::GraphicResourcePool pool;
        vsl::GraphicResource resource;
        std::optional<vsl::Shader<vsl::ShaderType::COMPUTE>> shader;
        std::optional<vsl::PipelineLayout> layout;
        std::optional<vsl::ComputePipeline> pipeline;
        void SetUp() override {
            VulkanIntegrationTest::SetUp();
            if (HasFatalFailure() || IsSkipped())
                return;
            if (!host_coherent_available())
                GTEST_SKIP() << "Host coherent memory unavailable";
            output.emplace(device, COUNT * sizeof(std::uint32_t));
            std::array<std::uint32_t, COUNT> zero{};
            ASSERT_TRUE(output->copy(zero));
            using namespace vsl::graphic_resource;
            binding.emplace(device, std::initializer_list<BindingPoint>{
                                        {0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 1}});
            resources.emplace(device);
            pool = resources->make(std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, 4}});
            auto [success, allocated] = pool.bind(std::vector<BindingLayout>{*binding});
            ASSERT_TRUE(success);
            ASSERT_EQ(allocated.size(), 1u);
            resource = allocated.front();
            resource.update(&*output);
            shader.emplace(device, shader_path("fill.comp.spv"));
            namespace pl = vsl::pipeline_layout;
            layout.emplace(device, pl::ShaderGroup("compute", {*shader}), pl::ResourceBinding{{*binding}},
                           pl::PushConstants(vsl::ShaderFlag::COMPUTE, {{vsl::ShaderFlag::COMPUTE, 0, 4}}));
            pipeline.emplace(*layout);
        }
        std::array<std::uint32_t, COUNT> execute(std::uint32_t base, bool explicit_binding = false,
                                                 bool explicit_push = false) {
            auto manager = make_manager();
            {
                auto phase = manager.start_phase<vsl::ComputePhase>();
                auto stream = phase << *pipeline;
                if (explicit_binding)
                    stream << vsl::command::BindGraphicResource(
                        std::vector<vsl::GraphicResource>{resource},
                        vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline);
                else
                    stream << vsl::command::BindGraphicResource(
                        resource, vsl::graphic_resource::BindingDestination::COMPUTE);
                if (explicit_push)
                    stream << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, sizeof(base), 0,
                                                         &base);
                else
                    stream << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base);
                stream << vsl::command::Dispatch(COUNT);
                // Make shader writes visible to the host before observing the payload.
                vk::MemoryBarrier barrier{};
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
                auto buffer = manager.get_buffer();
                buffer.data_->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {},
                    {});
            }
            device.data_->device.waitIdle();
            auto mapped = output->data(COUNT * sizeof(std::uint32_t));
            std::array<std::uint32_t, COUNT> actual{};
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            return actual;
        }
        static void ExpectSequence(const std::array<std::uint32_t, COUNT>& actual, std::uint32_t base) {
            for (std::size_t i = 0; i < COUNT; ++i)
                EXPECT_EQ(actual[i], base + i) << "index=" << i;
        }
        void TearDown() override {
            if (device.data_)
                device.data_->device.waitIdle();
            pipeline.reset();
            layout.reset();
            shader.reset();
            resource.data_.reset();
            // Pool and Resource hold each other strongly (I23). Break fixture-owned cycles.
            if (resources)
                for (auto& owned_pool : resources->data_->pools)
                    owned_pool->resources.clear();
            pool.data_.reset();
            resources.reset();
            binding.reset();
            output.reset();
            VulkanIntegrationTest::TearDown();
        }
    };
} // namespace vsl_test
