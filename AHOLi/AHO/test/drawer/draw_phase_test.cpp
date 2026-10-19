#include <AHO/drawer/draw_phase.hpp>
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
} // namespace phase_payload
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
} // namespace
#else
#include <gtest/gtest.h>
namespace {
    TEST(DrawPhaseTest, CurrentPhasePointerIsInitiallyEmpty) {
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
} // namespace
#endif
