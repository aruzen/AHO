#include <VSL/lifetime/graphics_access.hpp>
#include <VSL/lifetime/descriptor_access.hpp>
#include <VSL/lifetime/buffer_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include "support/compute_fixture.hpp"
#include "support/deferred_action.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::GraphicsAccess;
}

namespace {
    void make_host_visible(vsl::CommandBuffer buffer);
} // namespace

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::BufferAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    using BindGraphicResourceVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(BindGraphicResourceVulkanIntegrationTest, PublicTrackRejectsAnEmptyResourceBeforeRecordingItsUnrelatedUseGroup) {
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        auto use = vsl::ResourceUseGroup::ForDescriptor(1, resource);
        vsl::command::BindGraphicResource direct(vsl::GraphicResource{}, vsl::graphic_resource::BindingDestination::COMPUTE);
        vsl::command::BindGraphicResource grouped(vsl::GraphicResource{}, vsl::graphic_resource::BindingDestination::COMPUTE, std::nullopt, 0, use);
        EXPECT_THROW(direct.track(manager.get_buffer()), std::logic_error);
        EXPECT_THROW(grouped.track(manager.get_buffer()), std::logic_error);
        EXPECT_FALSE(DescriptorAccess::Owned(resource)->use.busy());
        phase.cancel();
    }
    TEST_F(BindGraphicResourceVulkanIntegrationTest, BothEntriesKeepTheirSnapshotsWhenPollingDestroysTheCommandAndContinuesCancelsOrStartsAPeer) {
        for (const bool track_only : {false, true}) {
            for (const bool tracked : {false, true}) {
                for (int action = 0; action < 3; ++action) {
                    SCOPED_TRACE(::testing::Message() << track_only << '/' << tracked << '/' << action);
                    const std::array<std::uint32_t, COUNT> zero{};
                    ASSERT_TRUE(output->copy(zero));
                    StorageBuffer alternate(device, sizeof(zero));
                    ASSERT_TRUE(alternate.copy(zero));
                    auto [success, replacement] = pool.bind(*binding);
                    ASSERT_TRUE(success);
                    replacement.update(&alternate);
                    auto use = tracked ? vsl::ResourceUseGroup::ForDescriptor(1, resource, vsl::ResourceRetention::COMPLETION) : nullptr;
                    auto command = std::make_unique<vsl::command::BindGraphicResource>(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline, 0, use);
                    auto previous = make_manager().independent(1), current = previous.independent(1);
                    auto submitted = previous.start_phase<vsl::ComputePhase>();
                    std::optional<vsl::ComputePhase> phase, peer;
                    const std::uint32_t base = 953;
                    bool called = false;
                    vsl_test::DeferredAction release([&] {
                        called = true;
                        command.reset();
                        if (action) {
                            phase->cancel();
                            if (action == 2) {
                                peer.emplace(current);
                                *peer << *pipeline << vsl::command::BindGraphicResource(replacement, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline)
                                      << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base);
                            }
                        }
                    });
                    vsl::ResourceUseGroup::ForDescriptor(1, resource, vsl::ResourceRetention::COMPLETION)->record(previous.get_buffer());
                    previous.get_buffer().retain_until_complete(release.owner());
                    ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                    device.data_->device.waitIdle();
                    phase.emplace(current);
                    *phase << *pipeline << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base);
                    const auto record = [&] {
                        if (track_only)
                            command->track(current.get_buffer());
                        else
                            command->invoke(current.get_pool(), current.get_buffer(), current);
                    };
                    if (action)
                        EXPECT_THROW(record(), std::logic_error);
                    else
                        EXPECT_NO_THROW(record());
                    EXPECT_TRUE(called);
                    EXPECT_FALSE(command);
                    if (!action) {
                        if (track_only)
                            *phase << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline);
                        *phase << vsl::command::Dispatch(COUNT);
                        make_host_visible(current.get_buffer());
                        ASSERT_EQ(phase->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                        ASSERT_TRUE(phase->completion().wait());
                    } else if (peer) {
                        *peer << vsl::command::Dispatch(COUNT);
                        make_host_visible(current.get_buffer());
                        ASSERT_EQ(peer->submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                        ASSERT_TRUE(peer->completion().wait());
                    }
                    std::array<std::uint32_t, COUNT> actual{};
                    auto mapped = output->data(sizeof(actual));
                    std::memcpy(actual.data(), mapped.data, sizeof(actual));
                    if (!action)
                        ExpectSequence(actual, base);
                    else
                        EXPECT_EQ(actual, zero);
                    auto other = alternate.data(sizeof(actual));
                    std::memcpy(actual.data(), other.data, sizeof(actual));
                    if (action == 2)
                        ExpectSequence(actual, base);
                    else
                        EXPECT_EQ(actual, zero);
                }
            }
        }
    }
    TEST_F(BindGraphicResourceVulkanIntegrationTest, BindKeepsItsStartingDescriptorsWhenPollingReplacesTheCommandInputs) {
        for (const bool tracked : {false, true}) {
            SCOPED_TRACE(tracked);
            const std::array<std::uint32_t, COUNT> zero{};
            ASSERT_TRUE(output->copy(zero));
            StorageBuffer alternate(device, sizeof(zero));
            ASSERT_TRUE(alternate.copy(zero));
            auto [success, replacement] = pool.bind(*binding);
            ASSERT_TRUE(success);
            replacement.update(&alternate);
            auto use = tracked ? vsl::ResourceUseGroup::ForDescriptor(1, resource, vsl::ResourceRetention::COMPLETION) : nullptr;
            auto command = std::make_unique<vsl::command::BindGraphicResource>(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline, 0, use);
            auto previous = make_manager().independent(1), current = previous.independent(1);
            auto submitted = previous.start_phase<vsl::ComputePhase>();
            bool called = false;
            vsl_test::DeferredAction release([&] {
                called = true;
                command->resources = {replacement};
            });
            vsl::ResourceUseGroup::ForDescriptor(1, resource, vsl::ResourceRetention::COMPLETION)->record(previous.get_buffer());
            previous.get_buffer().retain_until_complete(release.owner());
            ASSERT_EQ(submitted.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            device.data_->device.waitIdle();
            const std::uint32_t base = 379;
            auto phase = current.start_phase<vsl::ComputePhase>();
            phase << *pipeline << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base);
            command->invoke(current.get_pool(), current.get_buffer(), current);
            EXPECT_TRUE(called);
            phase << vsl::command::Dispatch(COUNT);
            make_host_visible(current.get_buffer());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
            std::array<std::uint32_t, COUNT> actual{};
            auto mapped = output->data(sizeof(actual));
            std::memcpy(actual.data(), mapped.data, sizeof(actual));
            ExpectSequence(actual, base);
            auto other = alternate.data(sizeof(actual));
            std::memcpy(actual.data(), other.data, sizeof(actual));
            EXPECT_EQ(actual, zero);
        }
    }
    TEST_F(BindGraphicResourceVulkanIntegrationTest, SingleAndVectorResourcesBindToComputeShader) {
        vsl::command::BindGraphicResource command(resource, vsl::graphic_resource::BindingDestination::COMPUTE);
        ASSERT_EQ(command.resources.size(), 1u);
        EXPECT_EQ(DescriptorAccess::Owned(command.resources.front()), DescriptorAccess::Owned(resource));
        EXPECT_EQ(command.destination, vsl::graphic_resource::BindingDestination::COMPUTE);
        EXPECT_EQ(command.first_binding, 0u);
        EXPECT_FALSE(command.pipeline.has_value());
        command.set_pipeline(*pipeline);
        ASSERT_TRUE(command.pipeline.has_value());
        EXPECT_EQ(GraphicsAccess::Owned(*(command.pipeline)), GraphicsAccess::Owned(*(pipeline)));
        ExpectSequence(execute(13), 13);
        ExpectSequence(execute(29, true), 29);
    }
} // namespace

namespace {
    class DescriptorResourceLifetimeVulkanIntegrationTest : public vsl_test::ComputeVulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    void make_host_visible(vsl::CommandBuffer buffer) {
        const vk::MemoryBarrier visible{.srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        const vsl::NativeRecordingContext recording(std::move(buffer));
        vk::CommandBuffer(recording.native_command_buffer()).pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
    }

    bool DescriptorResourceLifetimeVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, ExplicitPipelineBindsAcrossSeparateStreamExpressions) {
        expect_validation_abort("");
        for (const bool vector_resources : {false, true}) {
            for (const bool shared_command : {false, true}) {
                auto manager = make_manager();
                auto phase = manager.start_phase<vsl::ComputePhase>();
                phase << *pipeline;
                auto command = std::make_shared<vsl::command::BindGraphicResource>(
                    vector_resources ? vsl::command::BindGraphicResource(std::vector{resource}, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline)
                                     : vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline));
                if (shared_command)
                    phase << std::static_pointer_cast<vsl::command::Command>(command);
                else
                    phase << *command;
                std::uint32_t base = 503 + 10 * vector_resources + shared_command;
                phase << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                      << vsl::command::Dispatch(COUNT);
                vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                          .dstAccessMask = vk::AccessFlagBits::eHostRead};
                auto buffer = manager.get_buffer();
                CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
                ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                ASSERT_TRUE(phase.completion().wait());
                auto mapped = output->data();
                std::array<std::uint32_t, COUNT> actual;
                std::memcpy(actual.data(), mapped.data, sizeof(actual));
                ExpectSequence(actual, base);
            }
        }
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, MissingContextRestoresTheConstructorPipelineWithoutKeepingImplicitState) {
        vsl::ComputePipeline other(*layout);
        vsl::command::BindGraphicResource explicit_command(resource, vsl::graphic_resource::BindingDestination::COMPUTE, *pipeline);
        explicit_command.set_pipeline(other);
        ASSERT_TRUE(explicit_command.pipeline);
        EXPECT_EQ(GraphicsAccess::Owned(*(explicit_command.pipeline)), GraphicsAccess::Owned(other));
        explicit_command.set_pipeline(std::nullopt);
        ASSERT_TRUE(explicit_command.pipeline);
        EXPECT_EQ(GraphicsAccess::Owned(*(explicit_command.pipeline)), GraphicsAccess::Owned(*(pipeline)));
        vsl::command::BindGraphicResource implicit_command(resource, vsl::graphic_resource::BindingDestination::COMPUTE);
        implicit_command.set_pipeline(other);
        ASSERT_TRUE(implicit_command.pipeline);
        EXPECT_EQ(GraphicsAccess::Owned(*(implicit_command.pipeline)), GraphicsAccess::Owned(other));
        implicit_command.set_pipeline(std::nullopt);
        EXPECT_FALSE(implicit_command.pipeline);
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, EmptyBindingPreservesThePreviouslyBoundDescriptorSets) {
        // 検証エラーはテストを失敗させ、不正なコマンドをdriverへ渡さない。
        expect_validation_abort("");
        auto manager = make_manager();
        const std::uint32_t base = 421;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            phase << *pipeline
                  << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                  << vsl::command::BindGraphicResource(std::vector<vsl::GraphicResource>{}, vsl::graphic_resource::BindingDestination::COMPUTE)
                  << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                  << vsl::command::Dispatch(COUNT);
            const auto buffer = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                      .dstAccessMask = vk::AccessFlagBits::eHostRead};
            CommandAccess::Data(buffer)->command_buffers[CommandAccess::Data(buffer)->current_buffer_idx].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        auto mapped = output->data();
        std::array<std::uint32_t, COUNT> actual{};
        std::memcpy(actual.data(), mapped.data, sizeof(actual));
        ExpectSequence(actual, base);
    }
    TEST_F(DescriptorResourceLifetimeVulkanIntegrationTest, DispatchKeepsDescriptorBufferAndPipelineAliveUntilCompletion) {
        auto manager = make_manager();
        vsl::Completion completion;
        std::weak_ptr<vsl::impl::BufferImpl> weak_output = BufferAccess::Owned(*(output)).share();
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_resource = DescriptorAccess::Owned(resource).share();
        std::weak_ptr<vsl::impl::PipelineImpl> weak_pipeline = GraphicsAccess::Owned(*(pipeline)).share();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            completion = phase.completion();
            {
                auto stream = phase << *pipeline;
                std::uint32_t base = 307;
                stream << vsl::command::BindGraphicResource(resource, vsl::graphic_resource::BindingDestination::COMPUTE)
                       << vsl::command::PushConstant(*layout, vsl::ShaderFlag::COMPUTE, base)
                       << vsl::command::Dispatch(COUNT);
            }
            resource = {};
            pipeline.reset();
            layout.reset();
            binding.reset();
            resources.reset();
            DescriptorAccess::Owned(pool).reset();
            output.reset();
            EXPECT_FALSE(weak_resource.expired());
            EXPECT_FALSE(weak_pipeline.expired());
            auto buffer = manager.get_buffer();
            vk::MemoryBarrier barrier{.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                      .dstAccessMask = vk::AccessFlagBits::eHostRead};
            CommandAccess::Data(buffer)->command_buffers[buffer.get_current_buffer_idx()].pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, barrier, {}, {});
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        ASSERT_TRUE(completion.wait());
        auto output_owner = weak_output.lock();
        ASSERT_TRUE(output_owner);
        // holderで寿命を延ばさず、Completionが保持する既存のマップから結果を読む。
        ASSERT_TRUE(vsl::contain(output_owner->properties, vsl::MemoryProperty::HOST_COHERENT));
        const auto* values = static_cast<const std::uint32_t*>(output_owner->mapped);
        ASSERT_NE(values, nullptr);
        for (std::size_t i = 0; i < COUNT; ++i)
            EXPECT_EQ(values[i], 307u + i);
        output_owner.reset();
        completion = {};
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak_resource.expired());
        EXPECT_TRUE(weak_pipeline.expired());
        EXPECT_TRUE(weak_output.expired());
    }
} // namespace
