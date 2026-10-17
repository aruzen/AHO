#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
namespace {
using StandardEngineVulkanIntegration = aho_test::LiveGpuTest;
TEST_F(StandardEngineVulkanIntegration, ConstructorCreatesDeviceWindowAndFrameResources) {
    ASSERT_NO_THROW(start_engine());
    ASSERT_NE(engine->_data, nullptr);
    ASSERT_NE(engine->_data->logical_device._data, nullptr);
    EXPECT_FALSE(engine->_data->physical_device.name().empty());
    ASSERT_TRUE(engine->boot_window.has_value());
    auto window = engine->getWindow();
    ASSERT_NE(window._data->window_handle, nullptr);
    ASSERT_NE(window._data2, nullptr);
    EXPECT_EQ(window.window_size().value.x.value, 400);
    EXPECT_EQ(window.window_size().value.y.value, 400);
    EXPECT_GT(window.frame_size().value.x.value, 0);
    EXPECT_GT(window.frame_size().value.y.value, 0);
    EXPECT_NE(window._data2->surface, nullptr);
}
TEST_F(StandardEngineVulkanIntegration, ExplicitBootInitializesPreviouslyDeferredEngine) {
    ASSERT_NO_THROW(start_engine(false));
    EXPECT_EQ(engine->_data, nullptr);
    ASSERT_NO_THROW(engine->boot("AHO deferred boot"));
    ASSERT_TRUE(engine->boot_window.has_value());
    EXPECT_EQ(engine->getWindow().name(), "AHO deferred boot");
    EXPECT_EQ(engine->getWindow().engine._data, engine->_data);
}
}
#else
#include <gtest/gtest.h>
#include <AHO/engine/standard_engine.hpp>
namespace {
TEST(StandardEngineTest, DeferredBootAllocatesNoGpuOrWindowAndGetWindowReportsAbsence) {
    const auto previous = aho::engine::EngineAccessor::instance();
    {
        aho::StandardEngine engine("deferred test", false);
        EXPECT_EQ(engine._data, nullptr);
        EXPECT_FALSE(engine.boot_window.has_value());
        EXPECT_THROW(engine.getWindow(), std::bad_optional_access);
    }
    EXPECT_EQ(aho::engine::EngineAccessor::instance(), previous);
}
}
#endif
