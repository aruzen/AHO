#ifdef AHO_GPU_TESTS
#include "../../support/live_gpu.hpp"
#include <AHO/drawer/draw_phase.hpp>
#include <AHO/x/scene/render_target.hpp>
#include <AHO/x/screenshot.hpp>

namespace {
    using RenderTargetVulkanIntegration = aho_test::LiveGpuTest;

    TEST_F(RenderTargetVulkanIntegration, ResizeUpdatesTargetBeforeNotifyingItsRenderers) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        aho::x::scene::RenderTarget target(*engine);
        const auto pass = target.renderPass();
        const auto previous = target.swapchain();
        int callbacks = 0;
        auto subscription = target.onRecreated([&] {
            ++callbacks;
            EXPECT_EQ(target.swapchain()._data, window._data2->swapchain._data);
            const vsl::Viewport viewport(window._data2->swapchain);
            EXPECT_EQ(target.extent(), (std::array<unsigned, 2>{unsigned(viewport.width), unsigned(viewport.height)}));
        });
        EXPECT_TRUE(window.resize(480, 320));
        EXPECT_EQ(callbacks, 1);
        EXPECT_NE(target.swapchain()._data, previous._data);
        EXPECT_EQ(target.renderPass()._data, pass._data);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(callbacks, 2);
        EXPECT_EQ(target.renderPass()._data, pass._data);
    }

    TEST_F(RenderTargetVulkanIntegration, ScreenshotUsageChangeNotifiesTheSameTargetImmediately) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        aho::x::scene::RenderTarget target(*engine);
        const auto previous = target.swapchain();
        const auto size = target.extent();
        const auto pass = target.renderPass();
        int callbacks = 0;
        auto subscription = target.onRecreated([&] { ++callbacks; });
        EXPECT_FALSE(vsl::contain(previous.images.at(0).getUsage(), vsl::ImageType::TransferSrc));
        aho::x::Screenshot screenshot(window);
        EXPECT_EQ(callbacks, 1);
        EXPECT_NE(target.swapchain()._data, previous._data);
        EXPECT_EQ(target.swapchain()._data, window._data2->swapchain._data);
        EXPECT_EQ(target.extent(), size);
        EXPECT_EQ(target.renderPass()._data, pass._data);
        EXPECT_TRUE(vsl::contain(target.swapchain().images.at(0).getUsage(), vsl::ImageType::TransferSrc));
        aho::x::Screenshot second(window);
        EXPECT_EQ(callbacks, 1) << "Already enabled usage must not recreate the swapchain";
        {
            aho::DrawPhase phase(*engine, [](auto&) {});
            ASSERT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
            target.begin(phase);
            ASSERT_TRUE(screenshot.capture(phase));
        }
        auto pixels = screenshot.read();
        EXPECT_EQ(pixels.width, size[0]);
        EXPECT_EQ(pixels.height, size[1]);
    }

    TEST_F(RenderTargetVulkanIntegration, SubscriptionRemovalAndAdditionDuringNotificationAreSafe) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        aho::x::scene::RenderTarget target(*engine);
        int firstCalls = 0, removedCalls = 0, addedCalls = 0;
        aho::x::scene::RenderTarget::Subscription removed, added;
        bool installed = false;
        auto first = target.onRecreated([&] {
            ++firstCalls;
            removed = {};
            if (!installed) {
                installed = true;
                added = target.onRecreated([&] { ++addedCalls; });
            }
        });
        removed = target.onRecreated([&] { ++removedCalls; });
        EXPECT_TRUE(window.resize(480, 320));
        EXPECT_EQ(firstCalls, 1);
        EXPECT_EQ(removedCalls, 0);
        EXPECT_EQ(addedCalls, 0);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(firstCalls, 2);
        EXPECT_EQ(removedCalls, 0);
        EXPECT_EQ(addedCalls, 1);
        EXPECT_THROW(target.onRecreated({}), std::invalid_argument);
    }

    TEST_F(RenderTargetVulkanIntegration, TargetDestructionUnregistersItsPluginAndCallbacks) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto registrations = window._data->plugins.size();
        int calls = 0;
        aho::x::scene::RenderTarget::Subscription subscription;
        std::weak_ptr<vsl::PureWindow::Plugin> plugin;
        {
            aho::x::scene::RenderTarget target(*engine);
            EXPECT_EQ(window._data->plugins.size(), registrations + 1);
            for (const auto& [id, candidate] : window._data->plugins)
                if (std::dynamic_pointer_cast<aho::window::WindowResizeHookPlugin>(candidate))
                    plugin = candidate;
            subscription = target.onRecreated([&] { ++calls; });
            EXPECT_TRUE(window.resize(480, 320));
            EXPECT_EQ(calls, 1);
        }
        EXPECT_TRUE(plugin.expired());
        EXPECT_EQ(window._data->plugins.size(), registrations);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(calls, 1);
    }

    TEST_F(RenderTargetVulkanIntegration, ClosedWindowRejectsAccessAndStillAllowsPluginCleanup) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto registrations = window._data->plugins.size();
        {
            aho::x::scene::RenderTarget target(*engine);
            EXPECT_TRUE(window.close());
            EXPECT_THROW(target.extent(), std::logic_error);
            EXPECT_THROW(target.swapchain(), std::logic_error);
            EXPECT_THROW(window.recreateSwapchain(), std::logic_error);
        }
        EXPECT_EQ(window._data->plugins.size(), registrations);
    }

    TEST_F(RenderTargetVulkanIntegration, RecordingPhaseRejectsRecreationBeforeChangingTheSwapchain) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto previous = window._data2->swapchain;
        aho::DrawPhase phase(*engine);
        ASSERT_EQ(phase.state, vsl::ComputePhase::State::WaitSubmit);
        EXPECT_THROW(window.recreateSwapchain(vsl::ImageType::TransferSrc), std::logic_error);
        EXPECT_EQ(window._data2->swapchain._data, previous._data);
        EXPECT_THROW(aho::x::scene::RenderTarget target(*engine), std::logic_error);
    }
}
#endif
