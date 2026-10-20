#include <AHO/drawer/draw_phase.hpp>
#include <AHO/image_readback.hpp>
#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
#include "../support/commands.hpp"
namespace phase_payload {
    struct Value {
        int* calls;
        AHO_NAMESPACE::DrawPhase** received;
    };
    void draw(Value value, AHO_NAMESPACE::DrawPhase* phase) {
        ++*value.calls;
        *value.received = phase;
    }
} // namespace phase_payload
namespace {
    using DrawPhaseVulkanIntegration = aho_test::LiveGpuTest;
    TEST_F(DrawPhaseVulkanIntegration, PointerAndReferenceConstructorsSubmitFramesAndResetThreadLocal) {
        ASSERT_NO_THROW(start_engine());
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            AHO_NAMESPACE::DrawPhase phase(*engine);
            EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, &phase);
        }
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            AHO_NAMESPACE::DrawPhase phase(engine.get());
            EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, &phase);
        }
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(DrawPhaseVulkanIntegration, CustomCallbacksAndAllCommandOverloadsAreInvokedOnce) {
        ASSERT_NO_THROW(start_engine());
        int initialized = 0, finished = 0, commands = 0, delegates = 0, tuple_calls = 0;
        AHO_NAMESPACE::DrawPhase* tuple_phase = nullptr;
        {
            AHO_NAMESPACE::DrawPhase phase(*engine, [&](auto& stream) {
                ++initialized;
                stream << VSL_NAMESPACE::command::RenderPassBegin(engine->getWindow()._data2->render_pass, engine->getWindow()._data2->frame_buffer); }, [&](auto& stream) { ++finished; stream << VSL_NAMESPACE::command::RenderPassEnd(); });
            phase << aho_test::RecordingCommand(&commands);
            phase << aho_test::RecordingDelegate(&delegates);
            std::shared_ptr<VSL_NAMESPACE::command::__Command> command = std::make_shared<aho_test::RecordingCommand>(&commands);
            std::shared_ptr<VSL_NAMESPACE::command::__DelegateCommand> delegate = std::make_shared<aho_test::RecordingDelegate>(&delegates);
            phase << command;
            phase << delegate;
            phase_payload::Value payload{&tuple_calls, &tuple_phase};
            auto stream = phase << AHO_NAMESPACE::pack(payload);
            stream << aho_test::RecordingCommand(&commands);
            EXPECT_EQ(initialized, 1);
            EXPECT_EQ(finished, 0);
            EXPECT_EQ(commands, 3);
            EXPECT_EQ(delegates, 2);
            EXPECT_EQ(tuple_calls, 1);
            EXPECT_EQ(tuple_phase, &phase);
        }
        EXPECT_EQ(finished, 1);
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(DrawPhaseVulkanIntegration, RepeatedFramesWorkAcrossCommandBufferWraparound) {
        ASSERT_NO_THROW(start_engine());
        const auto frames = engine->_data->command_manager.getBuffer().getSize() + 2;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            SCOPED_TRACE(frame);
            AHO_NAMESPACE::DrawPhase phase(*engine);
            EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, &phase);
        }
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(DrawPhaseVulkanIntegration, ExplicitSubmissionRunsFinishOnceThroughBaseReferences) {
        ASSERT_NO_THROW(start_engine());
        for (unsigned split = 0; split < 2; ++split) {
            unsigned finished = 0;
            {
                // 終了コールバックが呼ばれない場合も安全に検証できるよう、初期化時にパスを閉じる。
                AHO_NAMESPACE::DrawPhase phase(*engine, [&](auto& stream) { stream << VSL_NAMESPACE::command::RenderPassBegin(engine->getWindow()._data2->render_pass,
                                                                                                                              engine->getWindow()._data2->frame_buffer)
                                                                                   << VSL_NAMESPACE::command::RenderPassEnd{}; }, [&](auto&) { ++finished; });
                if (split) {
                    VSL_NAMESPACE::DefaultPhase& base = phase;
                    EXPECT_EQ(base.submitCommands(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
                    EXPECT_EQ(finished, 1u);
                    EXPECT_EQ(base.present(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
                } else {
                    VSL_NAMESPACE::ComputePhase& base = phase;
                    EXPECT_EQ(base.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
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
            AHO_NAMESPACE::DrawPhase phase(*engine);
            if (split) {
                EXPECT_EQ(phase.submitCommands(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
                EXPECT_EQ(phase.present(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
            } else {
                EXPECT_EQ(phase.submit(), VSL_NAMESPACE::ComputePhase::SubmitResult::Success);
            }
            EXPECT_TRUE(phase.completion().wait());
        }
        engine->_data->command_manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, ExplicitThrowingFinishCancelsAndDoesNotRetryDuringDestruction) {
        ASSERT_NO_THROW(start_engine());
        unsigned calls = 0;
        {
            AHO_NAMESPACE::DrawPhase phase(*engine, nullptr, [&](auto&) {
                ++calls;
                throw std::runtime_error("explicit finish failed");
            });
            EXPECT_THROW(phase.submit(), std::runtime_error);
            EXPECT_EQ(calls, 1u);
        }
        EXPECT_EQ(calls, 1u);
        {
            AHO_NAMESPACE::DrawPhase next(*engine);
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
        auto chain = VSL_NAMESPACE::Swapchain(device, window._data2->surface, window._data2->swapchain,
                                              std::nullopt, std::nullopt,
                                              VSL_NAMESPACE::ImageType::ColorAttachment | VSL_NAMESPACE::ImageType::TransferSrc);
        window._data2->swapchain = chain;
        window._data2->frame_buffer = VSL_NAMESPACE::FrameBuffer(chain, window._data2->render_pass);
        if (window._data2->render_finished.size() != chain.getSwapImageSize())
            window._data2->render_finished = engine->_data->synchro_manager.createSemaphore("ticket-present", chain.getSwapImageSize());
        AHO_NAMESPACE::ImageReadback reader(manager);
        std::optional<AHO_NAMESPACE::ImageReadbackTicket> ticket;
        {
            AHO_NAMESPACE::DrawPhase phase(*engine, [&](auto& stream) {
                auto begin = VSL_NAMESPACE::command::RenderPassBegin(window._data2->render_pass, window._data2->frame_buffer);
                begin.clearValues[0] = VSL_NAMESPACE::ClearColor{1, 0, 0, 1};
                stream << begin; }, [&](auto& stream) {
                stream << VSL_NAMESPACE::command::RenderPassEnd{};
                ticket = reader.record(phase, chain.images.at(phase.getImageIndex()), VSL_NAMESPACE::ImageLayout::Present,
                                       {0, 0, 1, 1}, 57);
                ASSERT_TRUE(ticket);
                EXPECT_EQ(ticket->status(), VSL_NAMESPACE::Completion::Status::Pending); });
        }
        ASSERT_TRUE(ticket);
        EXPECT_EQ(ticket->info().frameId, 57u);
        auto pixels = ticket->read();
        const auto format = pixels.format.type();
        const bool bgra = format == VSL_NAMESPACE::data_format::___FormatTypes::Srgb8BGRA ||
                          format == VSL_NAMESPACE::data_format::___FormatTypes::UnsignedNormalized8BGRA;
        EXPECT_EQ(pixels.bytes[bgra ? 2 : 0], std::byte{255});
        EXPECT_EQ(pixels.bytes[1], std::byte{0});
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
        manager.waitIdle();
    }

    TEST_F(DrawPhaseVulkanIntegration, ThrowingFinishCallbackCancelsTicketAndAllowsNextFrame) {
        ASSERT_NO_THROW(start_engine());
        auto manager = engine->_data->command_manager;
        auto device = engine->_data->logical_device;
        VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::ColorAttachment | VSL_NAMESPACE::ImageType::TransferSrc> image(
            device, 8, 8, VSL_NAMESPACE::data_format::UnsignedNormalized8RGBA);
        VSL_NAMESPACE::RenderPassDescription description;
        description.addColor({.format = image.getFormat(), .finalLayout = VSL_NAMESPACE::ImageLayout::General});
        VSL_NAMESPACE::RenderPass pass(device, description);
        VSL_NAMESPACE::FrameBuffer target(pass, 8, 8, {image});
        AHO_NAMESPACE::ImageReadback reader(manager);
        std::optional<AHO_NAMESPACE::ImageReadbackTicket> ticket;
        {
            AHO_NAMESPACE::DrawPhase phase(*engine, [&](auto& stream) { stream << VSL_NAMESPACE::command::RenderPassBegin(target); }, [&](auto& stream) {
                    stream << VSL_NAMESPACE::command::RenderPassEnd{};
                    ticket = reader.record(phase, image, VSL_NAMESPACE::ImageLayout::General);
                    throw std::runtime_error("finish callback failed"); });
        }
        ASSERT_TRUE(ticket);
        EXPECT_EQ(ticket->status(), VSL_NAMESPACE::Completion::Status::Cancelled);
        EXPECT_THROW(ticket->read(), std::logic_error);
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            AHO_NAMESPACE::DrawPhase next(*engine);
        }
        manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, ThrowingInitializerRestoresCurrentPhaseAndAllowsNextFrame) {
        ASSERT_NO_THROW(start_engine());
        EXPECT_THROW((AHO_NAMESPACE::DrawPhase(*engine, [&](auto&) { throw std::runtime_error("initialization failed"); })), std::runtime_error);
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            AHO_NAMESPACE::DrawPhase next(*engine);
        }
        engine->_data->command_manager.waitIdle();
    }
    TEST_F(DrawPhaseVulkanIntegration, StackUnwindingCancelsFrameWithoutInvokingFinishCallback) {
        ASSERT_NO_THROW(start_engine());
        unsigned calls = 0;
        try {
            AHO_NAMESPACE::DrawPhase phase(*engine, nullptr, [&](auto&) { ++calls; });
            throw std::runtime_error("frame body failed");
        } catch (const std::runtime_error&) {
        }
        EXPECT_EQ(calls, 0u);
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
        {
            AHO_NAMESPACE::DrawPhase next(*engine);
        }
        engine->_data->command_manager.waitIdle();
    }

} // namespace
#else
#include <gtest/gtest.h>
namespace {
    TEST(DrawPhaseTest, CurrentPhasePointerIsInitiallyEmpty) {
        EXPECT_EQ(AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
} // namespace
#endif
