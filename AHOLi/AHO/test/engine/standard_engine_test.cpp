#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
namespace {
    using StandardEngineVulkanIntegration = aho_test::LiveGPUTest;
    TEST_F(StandardEngineVulkanIntegration, ConstructorCreatesDeviceWindowAndFrameResources) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_NE(engine->data_, nullptr);
        ASSERT_NE(engine->data_->logical_device.data_, nullptr);
        EXPECT_FALSE(engine->data_->physical_device.name().empty());
        ASSERT_TRUE(engine->boot_window.has_value());
        auto window = engine->get_window();
        ASSERT_NE(window.data_->window_handle, nullptr);
        ASSERT_NE(window.data2, nullptr);
        EXPECT_EQ(window.window_size().value.x.value, 400);
        EXPECT_EQ(window.window_size().value.y.value, 400);
        EXPECT_GT(window.frame_size().value.x.value, 0);
        EXPECT_GT(window.frame_size().value.y.value, 0);
        EXPECT_NE(window.data2->surface, nullptr);
    }
    TEST_F(StandardEngineVulkanIntegration, BasePointerDestructionReleasesClosedWindowAndEngineResources) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_TRUE(engine->boot_window.has_value());
        const std::weak_ptr<aho::engine::EngineData> engine_data = engine->data_;
        const std::weak_ptr<vsl::PureWindow::WindowData> window_data = engine->boot_window->data_;
        engine->data_->logical_device.data_->device.waitIdle();
        ASSERT_TRUE(engine->boot_window->close());
        std::unique_ptr<aho::engine::GraphicalEngine> base = std::move(engine);
        EXPECT_FALSE(engine_data.expired());
        EXPECT_FALSE(window_data.expired());
        base.reset();
        EXPECT_TRUE(engine_data.expired());
        EXPECT_TRUE(window_data.expired());
    }
    TEST_F(StandardEngineVulkanIntegration, ExplicitBootInitializesPreviouslyDeferredEngine) {
        ASSERT_NO_THROW(start_engine(false));
        EXPECT_EQ(engine->data_, nullptr);
        ASSERT_NO_THROW(engine->boot("AHO deferred boot"));
        ASSERT_TRUE(engine->boot_window.has_value());
        EXPECT_EQ(engine->get_window().name(), "AHO deferred boot");
        EXPECT_EQ(engine->get_window().engine.data_, engine->data_);
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/engine/standard_engine.hpp>
namespace {
    TEST(StandardEngineTest, DeferredBootAllocatesNoGPUOrWindowAndGetWindowReportsAbsence) {
        const auto previous = aho::engine::EngineAccessor::Instance();
        {
            aho::StandardEngine engine("deferred test", false);
            EXPECT_EQ(engine.data_, nullptr);
            EXPECT_FALSE(engine.boot_window.has_value());
            EXPECT_THROW(engine.get_window(), std::bad_optional_access);
        }
        EXPECT_EQ(aho::engine::EngineAccessor::Instance(), previous);
    }
} // namespace
#endif
