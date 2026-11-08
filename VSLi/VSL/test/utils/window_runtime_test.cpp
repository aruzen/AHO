#include "support/window_fixture.hpp"
#include <VSL/lifetime/window_runtime.hpp>
#include <VSL/window_plugin.hpp>
#include <thread>
#include <type_traits>

namespace {
    namespace l = vsl::lifetime;
    struct RetirementProbe : l::GLFWRetirement {
        std::vector<int>& events;
        int value;
        RetirementProbe* following = nullptr;
        std::thread::id disposed_on;
        RetirementProbe(std::vector<int>& events, int value) : events(events), value(value) { dispose = Dispose; }
        static void Dispose(l::GLFWRetirement* input) noexcept;
    };
    class WindowRuntimeVulkanIntegrationTest : public vsl_test::WindowIntegrationTest {};
    void RetirementProbe::Dispose(l::GLFWRetirement* input) noexcept {
        auto& probe = *static_cast<RetirementProbe*>(input);
        probe.disposed_on = std::this_thread::get_id();
        probe.events.push_back(probe.value);
        if (probe.following)
            l::retire_glfw(*probe.following);
    }
    TEST(WindowRuntimeTest, CallbackExitDoesNotDrainWithoutAnOuterNativeBoundary) {
        l::GLFWLease::Drain();
        std::vector<int> events;
        RetirementProbe retired(events, 1);
        {
            l::GLFWCallbackScope callback;
            l::retire_glfw(retired);
            EXPECT_TRUE(events.empty());
            EXPECT_THROW(l::GLFWLease::Drain(), std::logic_error);
            EXPECT_THROW(vsl::PureWindow::Update(), std::logic_error);
        }
        EXPECT_TRUE(events.empty());
        l::GLFWLease::Drain();
        EXPECT_EQ(events, std::vector<int>{1});
    }
    TEST(WindowRuntimeTest, OuterCallDrainsNestedAndNewlyRetiredObjectsInTheSameLoop) {
        l::GLFWLease::Drain();
        std::vector<int> events;
        RetirementProbe first(events, 1), second(events, 2);
        first.following = &second;
        {
            l::GLFWCallScope outer;
            {
                l::GLFWCallScope inner;
                l::GLFWCallbackScope callback;
                l::retire_glfw(first);
            }
            EXPECT_TRUE(events.empty());
        }
        EXPECT_EQ(events, (std::vector<int>{1, 2}));
    }
    TEST(WindowRuntimeTest, WorkerOnlyQueuesAndMainThreadPerformsDisposal) {
        l::GLFWLease::Drain();
        std::vector<int> events;
        RetirementProbe retired(events, 1);
        std::thread worker([&] { l::retire_glfw(retired); });
        worker.join();
        EXPECT_TRUE(events.empty());
        l::GLFWLease::Drain();
        EXPECT_EQ(events, std::vector<int>{1});
        EXPECT_EQ(retired.disposed_on, std::this_thread::get_id());
    }
    TEST(WindowRuntimeTest, NoThrowNativeUnregistrationDefersDisposalToAnExplicitBoundary) {
        static_assert(std::is_nothrow_constructible_v<l::GLFWCallScope, std::nothrow_t>);
        l::GLFWLease::Drain();
        std::vector<int> events;
        RetirementProbe retired(events, 1);
        {
            l::GLFWCallScope scope(std::nothrow);
            l::retire_glfw(retired);
        }
        EXPECT_TRUE(events.empty());
        l::GLFWLease::Drain();
        EXPECT_EQ(events, std::vector<int>{1});
    }
    TEST_F(WindowRuntimeVulkanIntegrationTest, LiveLeasesShareOneRuntimeAndDrainNeverForcesTheirRelease) {
        const auto a = l::GLFWLease::Acquire(), b = l::GLFWLease::Acquire();
        const auto first = a.owner_ref(), second = b.owner_ref();
        EXPECT_FALSE(first.owner_before(second));
        EXPECT_FALSE(second.owner_before(first));
        EXPECT_TRUE(first.managed());
        ASSERT_TRUE(window->close());
        EXPECT_NO_THROW(l::GLFWLease::Drain());
        EXPECT_TRUE(l::glfw_runtime_active());
    }
    TEST_F(WindowRuntimeVulkanIntegrationTest, RetiringRuntimeCannotBeReinitializedBeforeTheOuterScopeDrains) {
        ASSERT_TRUE(window->close());
        const auto previous = glfw_lease.owner_ref().weak();
        {
            l::GLFWCallScope outer;
            std::thread worker([owner = std::move(glfw_lease)]() mutable { owner = {}; });
            worker.join();
            EXPECT_TRUE(previous.expired());
            EXPECT_TRUE(l::glfw_runtime_active());
            try {
                (void)l::GLFWLease::Acquire();
                ADD_FAILURE() << "A retiring runtime was reinitialized";
            } catch (const std::logic_error& error) {
                EXPECT_NE(std::string(error.what()).find("retiring"), std::string::npos);
            }
        }
        EXPECT_FALSE(l::glfw_runtime_active());
        glfw_lease = l::GLFWLease::Acquire();
        EXPECT_TRUE(l::glfw_runtime_active());
    }
    TEST_F(WindowRuntimeVulkanIntegrationTest, UnregisteredRetirementCannotEraseTheCurrentRuntime) {
        ASSERT_TRUE(window->close());
        glfw_lease = {};
        l::GLFWLease::Drain();
        ASSERT_FALSE(l::glfw_runtime_active());
        {
            l::GLFWCallScope outer;
            // control block確保失敗で、登録前の実体だけが退役した場合を再現する。
            auto* unpublished = new vsl::impl::GLFWRuntimeImpl;
            unpublished->node.retire();
            l::retire_glfw(*unpublished);
            glfw_lease = l::GLFWLease::Acquire();
        }
        EXPECT_TRUE(l::glfw_runtime_active());
        const auto owner = glfw_lease.owner_ref();
        const auto next = l::GLFWLease::Acquire().owner_ref();
        EXPECT_FALSE(owner.owner_before(next));
        EXPECT_FALSE(next.owner_before(owner));
    }
    TEST_F(WindowRuntimeVulkanIntegrationTest, NativeCallbackCloseQueuesTheWindowUntilTheCallerReturns) {
        const auto os = window->window_owner().owner_ref().weak();
        auto hook = window->add_plugin<vsl::window_plugin::HookResize>([os](vsl::PureWindow current, int, int) {
            EXPECT_TRUE(current.close());
            EXPECT_FALSE(os.expired());
            EXPECT_THROW(vsl::PureWindow::NoCheckUpdate(), std::logic_error);
        });
        auto* native = static_cast<GLFWwindow*>(window->data_->window_handle);
        const auto callback = glfwSetFramebufferSizeCallback(native, nullptr);
        ASSERT_NE(callback, nullptr);
        callback(native, 80, 64);
        EXPECT_FALSE(static_cast<bool>(*window));
        EXPECT_TRUE(os.expired());
        EXPECT_GT(l::pending_glfw_retirements(), 0u);
        l::GLFWLease::Drain();
        EXPECT_EQ(l::pending_glfw_retirements(), 0u);
    }
} // namespace
