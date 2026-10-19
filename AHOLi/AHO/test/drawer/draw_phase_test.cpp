#include <AHO/drawer/draw_phase.hpp>
#include <AHO/image_readback.hpp>
#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include "../support/commands.hpp"
namespace phase_payload {
    struct Value {
        int* calls;
        aho::DrawPhase** received;
    };
    void draw(Value value, aho::DrawPhase* phase) {
        ++*value.calls;
        *value.received = phase;
    }
}
namespace {
    using DrawPhaseVulkanIntegration = aho_test::LiveGpuTest;
    TEST_F(DrawPhaseVulkanIntegration, PointerAndReferenceConstructorsSubmitFramesAndResetThreadLocal) {
        ASSERT_NO_THROW(start_engine());
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            aho::DrawPhase phase(*engine);
            EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &phase);
        }
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            aho::DrawPhase phase(engine.get());
            EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &phase);
        }
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(DrawPhaseVulkanIntegration, CustomCallbacksAndAllCommandOverloadsAreInvokedOnce) {
        ASSERT_NO_THROW(start_engine());
        int initialized = 0, finished = 0, commands = 0, delegates = 0, tuple_calls = 0;
        aho::DrawPhase* tuple_phase = nullptr;
        {
            aho::DrawPhase phase(*engine, [&](auto& stream) {
                ++initialized;
                stream << vsl::command::RenderPassBegin(engine->getWindow()._data2->render_pass, engine->getWindow()._data2->frame_buffer); }, [&](auto& stream) { ++finished; stream << vsl::command::RenderPassEnd(); });
            phase << aho_test::RecordingCommand(&commands);
            phase << aho_test::RecordingDelegate(&delegates);
            std::shared_ptr<vsl::command::__Command> command = std::make_shared<aho_test::RecordingCommand>(&commands);
            std::shared_ptr<vsl::command::__DelegateCommand> delegate = std::make_shared<aho_test::RecordingDelegate>(&delegates);
            phase << command;
            phase << delegate;
            phase_payload::Value payload{&tuple_calls, &tuple_phase};
            auto stream = phase << aho::pack(payload);
            stream << aho_test::RecordingCommand(&commands);
            EXPECT_EQ(initialized, 1);
            EXPECT_EQ(finished, 0);
            EXPECT_EQ(commands, 3);
            EXPECT_EQ(delegates, 2);
            EXPECT_EQ(tuple_calls, 1);
            EXPECT_EQ(tuple_phase, &phase);
        }
        EXPECT_EQ(finished, 1);
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(DrawPhaseVulkanIntegration, RepeatedFramesWorkAcrossCommandBufferWraparound) {
        ASSERT_NO_THROW(start_engine());
        const auto frames = engine->_data->command_manager.getBuffer().getSize() + 2;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            SCOPED_TRACE(frame);
            aho::DrawPhase phase(*engine);
            EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &phase);
        }
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(DrawPhaseVulkanIntegration, ExplicitSubmissionRunsFinishOnceThroughBaseReferences) {
        ASSERT_NO_THROW(start_engine());
        for (unsigned split = 0; split < 2; ++split) {
            unsigned finished = 0;
            {
                // End the pass in initialize so the pre-fix skipped callback can be tested safely.
                aho::DrawPhase phase(*engine, [&](auto& stream) { stream << vsl::command::RenderPassBegin(engine->getWindow()._data2->render_pass,
                                                                                                          engine->getWindow()._data2->frame_buffer)
                                                                         << vsl::command::RenderPassEnd{}; }, [&](auto&) { ++finished; });
                if (split) {
                    vsl::DefaultPhase& base = phase;
                    EXPECT_EQ(base.submitCommands(), vsl::ComputePhase::SubmitResult::Success);
                    EXPECT_EQ(finished, 1u);
                    EXPECT_EQ(base.present(), vsl::ComputePhase::SubmitResult::Success);
                } else {
                    vsl::ComputePhase& base = phase;
                    EXPECT_EQ(base.submit(), vsl::ComputePhase::SubmitResult::Success);
                    EXPECT_EQ(finished, 1u);
                }
                EXPECT_TRUE(phase.completion().wait());
            }
            EXPECT_EQ(finished, 1u);
        }
        engine->_data->command_manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, ExplicitSubmissionClosesTheDefaultRenderPass) {
        ASSERT_NO_THROW(start_engine());
        for (unsigned split = 0; split < 2; ++split) {
            aho::DrawPhase phase(*engine);
            if (split) {
                EXPECT_EQ(phase.submitCommands(), vsl::ComputePhase::SubmitResult::Success);
                EXPECT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::Success);
            } else {
                EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::Success);
            }
            EXPECT_TRUE(phase.completion().wait());
        }
        engine->_data->command_manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, ExplicitThrowingFinishCancelsAndDoesNotRetryDuringDestruction) {
        ASSERT_NO_THROW(start_engine());
        unsigned calls = 0;
        {
            aho::DrawPhase phase(*engine, nullptr, [&](auto&) {
                ++calls;
                throw std::runtime_error("explicit finish failed");
            });
            EXPECT_THROW(phase.submit(), std::runtime_error);
            EXPECT_EQ(calls, 1u);
        }
        EXPECT_EQ(calls, 1u);
        {
            aho::DrawPhase next(*engine);
        }
        engine->_data->command_manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, FinishCallbackRecordsReadbackBeforeSubmitAndPresent) {
        ASSERT_NO_THROW(start_engine());
        auto window = engine->getWindow();
        auto manager = engine->_data->command_manager;
        auto device = engine->_data->logical_device;
        const auto capabilities = device._data->parentDevice->device.getSurfaceCapabilitiesKHR(window._data2->surface->_data->surface);
        if (!(capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
            GTEST_SKIP() << "Swapchain does not support TransferSrc";
        auto chain = vsl::Swapchain(device, window._data2->surface, window._data2->swapchain,
                                    std::nullopt, std::nullopt,
                                    vsl::ImageType::ColorAttachment | vsl::ImageType::TransferSrc);
        window._data2->swapchain = chain;
        window._data2->frame_buffer = vsl::FrameBuffer(chain, window._data2->render_pass);
        if (window._data2->render_finished.size() != chain.getSwapImageSize())
            window._data2->render_finished = engine->_data->synchro_manager.createSemaphore("ticket-present", chain.getSwapImageSize());
        aho::ImageReadback reader(manager);
        std::optional<aho::ImageReadbackTicket> ticket;
        {
            aho::DrawPhase phase(*engine, [&](auto& stream) {
                auto begin = vsl::command::RenderPassBegin(window._data2->render_pass, window._data2->frame_buffer);
                begin.clearValues[0] = vsl::ClearColor{1, 0, 0, 1};
                stream << begin; }, [&](auto& stream) {
                stream << vsl::command::RenderPassEnd{};
                ticket = reader.record(phase, chain.images.at(phase.getImageIndex()), vsl::ImageLayout::Present,
                                       {0, 0, 1, 1}, 57);
                ASSERT_TRUE(ticket);
                EXPECT_EQ(ticket->status(), vsl::Completion::Status::Pending); });
        }
        ASSERT_TRUE(ticket);
        EXPECT_EQ(ticket->info().frameId, 57u);
        auto pixels = ticket->read();
        const auto format = pixels.format.type();
        const bool bgra = format == vsl::data_format::___FormatTypes::Srgb8BGRA ||
                          format == vsl::data_format::___FormatTypes::UnsignedNormalized8BGRA;
        EXPECT_EQ(pixels.bytes[bgra ? 2 : 0], std::byte{255});
        EXPECT_EQ(pixels.bytes[1], std::byte{0});
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        manager.waitIdle();
    }

    TEST_F(DrawPhaseVulkanIntegration, ThrowingFinishCallbackCancelsTicketAndAllowsNextFrame) {
        ASSERT_NO_THROW(start_engine());
        auto manager = engine->_data->command_manager;
        auto device = engine->_data->logical_device;
        vsl::Image<vsl::ImageType::ColorAttachment | vsl::ImageType::TransferSrc> image(
            device, 8, 8, vsl::data_format::UnsignedNormalized8RGBA);
        vsl::RenderPassDescription description;
        description.addColor({.format = image.getFormat(), .finalLayout = vsl::ImageLayout::General});
        vsl::RenderPass pass(device, description);
        vsl::FrameBuffer target(pass, 8, 8, {image});
        aho::ImageReadback reader(manager);
        std::optional<aho::ImageReadbackTicket> ticket;
        {
            aho::DrawPhase phase(*engine, [&](auto& stream) { stream << vsl::command::RenderPassBegin(target); }, [&](auto& stream) {
                    stream << vsl::command::RenderPassEnd{};
                    ticket = reader.record(phase, image, vsl::ImageLayout::General);
                    throw std::runtime_error("finish callback failed"); });
        }
        ASSERT_TRUE(ticket);
        EXPECT_EQ(ticket->status(), vsl::Completion::Status::Cancelled);
        EXPECT_THROW(ticket->read(), std::logic_error);
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            aho::DrawPhase next(*engine);
        }
        manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, ThrowingInitializerRestoresCurrentPhaseAndAllowsNextFrame) {
        ASSERT_NO_THROW(start_engine());
        EXPECT_THROW((aho::DrawPhase(*engine, [&](auto&) { throw std::runtime_error("initialization failed"); })), std::runtime_error);
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            aho::DrawPhase next(*engine);
        }
        engine->_data->command_manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, StackUnwindingCancelsFrameWithoutInvokingFinishCallback) {
        ASSERT_NO_THROW(start_engine());
        unsigned calls = 0;
        try {
            aho::DrawPhase phase(*engine, nullptr, [&](auto&) { ++calls; });
            throw std::runtime_error("frame body failed");
        } catch (const std::runtime_error&) {
        }
        EXPECT_EQ(calls, 0u);
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            aho::DrawPhase next(*engine);
        }
        engine->_data->command_manager.waitIdle();
    }

}
#else
#include <gtest/gtest.h>
namespace {
    TEST(DrawPhaseTest, CurrentPhasePointerIsInitiallyEmpty) {
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
}
#endif
