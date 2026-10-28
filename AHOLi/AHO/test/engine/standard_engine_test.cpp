#ifdef AHO_GPU_TESTS
#include "../support/live_gpu.hpp"
namespace {
    using StandardEngineVulkanIntegration = aho_test::LiveGPUTest;
    TEST_F(StandardEngineVulkanIntegration, ConstructorCreatesDeviceWindowAndFrameResources) {
        ASSERT_NO_THROW(start_engine());
        ASSERT_NE(engine->data(), nullptr);
        ASSERT_NE(engine->data()->logical_device.data_, nullptr);
        EXPECT_FALSE(engine->data()->physical_device.name().empty());
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
        const std::weak_ptr<aho::engine::EngineData> engine_data = engine->data();
        const std::weak_ptr<vsl::PureWindow::WindowData> window_data = engine->boot_window->data_;
        engine->data()->logical_device.data_->device.waitIdle();
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
        EXPECT_EQ(engine->data(), nullptr);
        ASSERT_NO_THROW(engine->boot("AHO deferred boot"));
        ASSERT_TRUE(engine->boot_window.has_value());
        EXPECT_EQ(engine->get_window().name(), "AHO deferred boot");
        EXPECT_EQ(engine->get_window().engine.data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, MovedRegistrationAndHandleKeepTheDefaultAndRestoreThePreviousOne) {
        ASSERT_NO_THROW(start_engine());
        auto alternate = std::make_shared<aho::engine::EngineData>(*engine->data());
        {
            aho::engine::EngineAccessor handle(alternate);
            aho::engine::DefaultEngineRegistration registration(handle);
            auto moved_handle = std::move(handle);
            auto moved_registration = std::move(registration);
            EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), alternate);
            EXPECT_EQ(moved_handle.data(), alternate);
            EXPECT_FALSE(handle.data());
        }
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, RemovingAnOlderRegistrationKeepsTheNewestRegistration) {
        ASSERT_NO_THROW(start_engine());
        std::optional<aho::engine::DefaultEngineRegistration> older(std::in_place, *engine);
        const auto alternate = std::make_shared<aho::engine::EngineData>(*engine->data());
        std::optional<aho::engine::DefaultEngineRegistration> newer(std::in_place, aho::engine::EngineAccessor(alternate));
        older.reset();
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), alternate);
        newer.reset();
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, DefaultRegistrationRetainsItsDataUntilTheRegistrationEnds) {
        ASSERT_NO_THROW(start_engine());
        std::weak_ptr<aho::engine::EngineData> data;
        std::optional<aho::engine::DefaultEngineRegistration> registration;
        {
            auto alternate = std::make_shared<aho::engine::EngineData>(*engine->data());
            data = alternate;
            registration.emplace(aho::engine::EngineAccessor(std::move(alternate)));
        }
        EXPECT_FALSE(data.expired());
        registration.reset();
        EXPECT_TRUE(data.expired());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), engine->data());
    }

    TEST_F(StandardEngineVulkanIntegration, SharedHandleRetainsResourcesAfterTheOwnerUnregisters) {
        ASSERT_NO_THROW(start_engine());
        auto handle = aho::engine::EngineAccessor(*engine);
        const std::weak_ptr<aho::engine::EngineData> data = handle.data();
        engine->data()->command_manager.wait_idle();
        ASSERT_TRUE(engine->get_window().close());
        engine.reset();
        EXPECT_FALSE(aho::engine::DefaultEngineRegistration::Current().data());
        EXPECT_FALSE(data.expired());
        handle = {};
        EXPECT_TRUE(data.expired());
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/engine/standard_engine.hpp>
namespace {
    TEST(StandardEngineTest, DeferredBootAllocatesNoGPUOrWindowAndGetWindowReportsAbsence) {
        const auto previous = aho::engine::DefaultEngineRegistration::Current().data();
        {
            aho::StandardEngine engine("deferred test", false);
            EXPECT_EQ(engine.data(), nullptr);
            EXPECT_FALSE(engine.boot_window.has_value());
            EXPECT_THROW(engine.get_window(), std::bad_optional_access);
        }
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), previous);
    }
} // namespace
#endif
