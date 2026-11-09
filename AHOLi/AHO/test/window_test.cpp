#ifdef AHO_GPU_TESTS
#include <VSL/lifetime/swapchain_access.hpp>
#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/image_access.hpp>
#include <VSL/lifetime/graphics_access.hpp>
#include "support/live_gpu.hpp"
#include "support/frame_buffer_query.hpp"
#include <AHO/drawer/draw_phase.hpp>
#include <AHO/io/mouse.hpp>
#include <AHO/io/keyboard.hpp>
#include <VSL/window_plugin.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/operation/frame_buffer_ops.hpp>
#include <VSL/operation/swapchain_ops.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/lifetime/surface_recovery.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif
namespace {
    using AHOWindowVulkanIntegration = aho_test::LiveGPUTest;

    struct CloseTargetEngine : aho::engine::GraphicalEngine {
        aho::Window selected;
        CloseTargetEngine(aho::engine::EngineAccessor runtime, aho::Window window);
        aho::Window get_window() override { return selected; }
    };
    struct CloseNativeProbe {
        unsigned nested = 0, drains = 0;
        bool active = false;
        std::function<void()> on_submit, on_present, on_acquire, on_reset;
        static VkResult Submit(void*, VkQueue, const VkSubmitInfo*, VkFence) noexcept;
        static VkResult Idle(void*, VkDevice) noexcept;
        static VkResult Present(void*, VkQueue, const VkPresentInfoKHR*) noexcept;
        static VkResult Release(void*, VkDevice, const VkReleaseSwapchainImagesInfoEXT*) noexcept;
        static VkResult Acquire(void*, VkDevice, VkSwapchainKHR, std::uint64_t, VkSemaphore, VkFence, std::uint32_t*) noexcept;
        static VkResult Reset(void*, VkDevice, std::uint32_t, const VkFence*) noexcept;
        void callback(std::function<void()>& function);
    };
    struct CloseDestructorHook : vsl::PureWindow::Plugin {
        std::function<void()> callback;
        CloseDestructorHook(vsl::PureWindow*, std::function<void()> callback) : callback(std::move(callback)) {}
        ~CloseDestructorHook() override { callback(); }
    };
    struct FixedIdResizeHook : aho::window::WindowResizeHookPlugin {
        FixedIdResizeHook(vsl::PureWindow* window, unsigned char id, std::function<void(aho::Window*)> callback)
            : WindowResizeHookPlugin(window, std::move(callback)) { this->id = id; }
    };
    struct GenerationProbe {
        unsigned creates = 0, frames = 0, frame_destroys = 0;
        bool fail_create = false, fail_frame = false;
        VkSwapchainKHR previous = VK_NULL_HANDLE;
        static VkResult Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output);
        static vk::Framebuffer Frame(void* context, vk::Device device, const vk::FramebufferCreateInfo& info);
        static void DestroyFrame(void* context, vk::Device device, vk::Framebuffer frame) noexcept;
    };
    CloseTargetEngine::CloseTargetEngine(aho::engine::EngineAccessor runtime, aho::Window window)
        : selected(std::move(window)) {
        reset_data(std::make_shared<aho::engine::EngineData>(*runtime.data()));
        data()->command_manager = runtime.data()->command_manager.independent(1);
    }
    void CloseNativeProbe::callback(std::function<void()>& function) {
        if (auto call = std::exchange(function, {})) {
            active = true;
            call();
            active = false;
        }
    }
    VkResult CloseNativeProbe::Submit(void* context, VkQueue queue, const VkSubmitInfo* info, VkFence fence) noexcept {
        auto& probe = *static_cast<CloseNativeProbe*>(context);
        probe.nested += probe.active;
        probe.drains += !info->commandBufferCount && info->waitSemaphoreCount;
        const auto result = vkQueueSubmit(queue, 1, info, fence);
        probe.callback(probe.on_submit);
        return result;
    }
    VkResult CloseNativeProbe::Idle(void* context, VkDevice device) noexcept {
        auto& probe = *static_cast<CloseNativeProbe*>(context);
        probe.nested += probe.active;
        return vkDeviceWaitIdle(device);
    }
    VkResult CloseNativeProbe::Present(void* context, VkQueue queue, const VkPresentInfoKHR* info) noexcept {
        auto& probe = *static_cast<CloseNativeProbe*>(context);
        probe.nested += probe.active;
        const auto result = vkQueuePresentKHR(queue, info);
        probe.callback(probe.on_present);
        return result;
    }
    VkResult CloseNativeProbe::Release(void* context, VkDevice device, const VkReleaseSwapchainImagesInfoEXT* info) noexcept {
        auto& probe = *static_cast<CloseNativeProbe*>(context);
        probe.nested += probe.active;
        const auto release = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(vkGetDeviceProcAddr(device, "vkReleaseSwapchainImagesEXT"));
        return release ? release(device, info) : VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkResult CloseNativeProbe::Acquire(void* context, VkDevice device, VkSwapchainKHR chain, std::uint64_t timeout,
                                       VkSemaphore semaphore, VkFence fence, std::uint32_t* image) noexcept {
        auto& probe = *static_cast<CloseNativeProbe*>(context);
        probe.nested += probe.active;
        const auto result = vkAcquireNextImageKHR(device, chain, timeout, semaphore, fence, image);
        probe.callback(probe.on_acquire);
        return result;
    }
    VkResult CloseNativeProbe::Reset(void* context, VkDevice device, std::uint32_t count, const VkFence* fences) noexcept {
        auto& probe = *static_cast<CloseNativeProbe*>(context);
        probe.nested += probe.active;
        const auto result = vkResetFences(device, count, fences);
        probe.callback(probe.on_reset);
        return result;
    }

    void expect_closed_resources(const std::shared_ptr<aho::window::WindowData>& data) {
        ASSERT_TRUE(data);
        EXPECT_TRUE(data->closed);
        EXPECT_FALSE(data->surface);
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(data->swapchain));
        EXPECT_TRUE(data->swapchain.images().empty());
        EXPECT_FALSE(data->render_pass);
        EXPECT_FALSE(data->frame_buffer);
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(data->image_available));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(data->render_finished));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(data->in_flight));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(data->acquire_fences));
        EXPECT_FALSE(data->standard_draw);
        EXPECT_FALSE(data->pending_generation);
        EXPECT_FALSE(data->deferred_usage);
    }
    TEST_F(AHOWindowVulkanIntegration, ClosingAfterPresentationReleasesGraphicsEvenWhenWindowCopiesAndContextsRemain) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto copy = window;
        const auto context = window.render_context();
        const auto drawing = window.data2;
        const std::weak_ptr<void> generation = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(drawing->swapchain).share();
        const std::weak_ptr<void> frames = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(drawing->image_available).share();
        const std::weak_ptr<void> surface = drawing->surface->data_;
        {
            aho::DrawPhase phase(*engine);
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_TRUE(window.close());
        expect_closed_resources(drawing);
        EXPECT_FALSE(static_cast<bool>(copy));
        EXPECT_THROW(context.render_pass(), std::logic_error);
        EXPECT_THROW(context.commands(), std::logic_error);
        EXPECT_TRUE(generation.expired());
        EXPECT_TRUE(frames.expired());
        EXPECT_TRUE(surface.expired());
        EXPECT_TRUE(engine->data());
    }
    TEST_F(AHOWindowVulkanIntegration, TitlebarCloseUsesTheGraphicsPathForSharedDataWindows) {
        ASSERT_NO_THROW(start_engine());
        auto& original = *engine->boot_window;
        aho::Window copy(*engine, original.data_, original.data2);
        const auto drawing = copy.data2;
        const std::weak_ptr<void> owner = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(drawing->swapchain).share();
        glfwSetWindowShouldClose(static_cast<GLFWwindow*>(copy.data_->window_handle), GLFW_TRUE);
        EXPECT_TRUE(vsl::PureWindow::Update());
        expect_closed_resources(drawing);
        EXPECT_FALSE(static_cast<bool>(copy));
        EXPECT_FALSE(static_cast<bool>(original));
        EXPECT_TRUE(owner.expired());
        EXPECT_FALSE(copy.close());
        EXPECT_FALSE(copy.data2);
    }
    TEST_F(AHOWindowVulkanIntegration, ClosingARecordingPhaseCancelsWithoutCallingItsFinishCallback) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        unsigned finishes = 0;
        auto phase = std::make_unique<aho::DrawPhase>(*engine, nullptr, [&](auto&) { ++finishes; });
        const auto completion = phase->completion();
        const auto drawing = window.data2;
        EXPECT_TRUE(window.close());
        expect_closed_resources(drawing);
        EXPECT_FALSE(phase->recording());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_EQ(phase->submit(), vsl::ComputePhase::SubmitResult::FAILED);
        phase.reset();
        EXPECT_EQ(finishes, 0u);
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(AHOWindowVulkanIntegration, TheWeakClosePluginStillClosesANativeWindowAfterItsDrawingMetadataExpires) {
        ASSERT_NO_THROW(start_engine());
        std::optional<vsl::PureWindow> native;
        std::weak_ptr<aho::window::WindowData> drawing;
        {
            aho::Window secondary("abandoned facade", 64, 48, *engine);
            native.emplace(secondary.data_);
            drawing = secondary.data2;
        }
        EXPECT_TRUE(drawing.expired());
        ASSERT_TRUE(static_cast<bool>(*native));
        glfwSetWindowShouldClose(static_cast<GLFWwindow*>(native->data_->window_handle), GLFW_TRUE);
        EXPECT_TRUE(vsl::PureWindow::Update());
        EXPECT_FALSE(static_cast<bool>(*native));
    }
    TEST_F(AHOWindowVulkanIntegration, PluginDestructionCancelsADeletedPhaseBeforeTheTLSCancellationPass) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        unsigned finishes = 0;
        auto phase = std::make_unique<aho::DrawPhase>(*engine, nullptr, [&](auto&) { ++finishes; });
        const auto result = phase->completion();
        window.add_plugin<CloseDestructorHook>([&] { phase.reset(); });
        EXPECT_TRUE(window.close());
        EXPECT_FALSE(phase);
        EXPECT_EQ(finishes, 0u);
        EXPECT_EQ(result.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, nullptr);
    }
    TEST_F(AHOWindowVulkanIntegration, ResourceDestructionCanDeleteTheClosingFacadeAfterEverySharedFieldIsDetached) {
        ASSERT_NO_THROW(start_engine());
        auto window = std::make_unique<aho::Window>("self deleting close", 64, 48, *engine);
        const auto drawing = window->data2;
        bool destroyed = false;
        drawing->standard_draw = std::shared_ptr<aho::drawer::detail::StandardDrawState>(nullptr, [&](aho::drawer::detail::StandardDrawState*) {
            expect_closed_resources(drawing);
            destroyed = true;
            window.reset();
        });
        EXPECT_TRUE(window->close());
        EXPECT_TRUE(destroyed);
        EXPECT_FALSE(window);
    }
    TEST_F(AHOWindowVulkanIntegration, ForeignSurfaceNativeCallsDeferCloseRecoveryUntilTheirResultsAreCommitted) {
        ASSERT_NO_THROW(start_engine());
        for (const bool presenting : {false, true}) {
            for (const bool orphan : {false, true}) {
                SCOPED_TRACE(testing::Message() << "present=" << presenting << " orphan=" << orphan);
                aho::Window a("retiring A", 64, 48, *engine), b("active B", 64, 48, *engine);
                auto runtime = std::make_unique<CloseTargetEngine>(*engine, a);
                CloseTargetEngine other(*engine, b);
                auto phase = std::make_unique<aho::DrawPhase>(*runtime);
                const auto result = phase->completion();
                const auto drawing = a.data2;
                const std::weak_ptr<void> generation = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(drawing->swapchain).share();
                const std::weak_ptr<void> synchronization = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(drawing->image_available).share();
                aho::DrawPhase native(other);
                CloseNativeProbe probe;
                bool callback = false;
                auto close = [&] {
                    callback = true;
                    EXPECT_TRUE(a.close());
                    expect_closed_resources(drawing);
                    EXPECT_EQ(result.status(), vsl::Completion::Status::CANCELLED);
                    if (orphan) {
                        phase.reset();
                        runtime.reset();
                    }
                    EXPECT_FALSE(generation.expired());
                    EXPECT_FALSE(synchronization.expired());
                    EXPECT_EQ(probe.drains, 0u);
                    EXPECT_EQ(probe.nested, 0u);
                };
                (presenting ? probe.on_present : probe.on_submit) = close;
                const vsl::operation::ScopedSubmissionNativeCalls submissions({.context = &probe, .wait_idle = CloseNativeProbe::Idle, .queue_submit = CloseNativeProbe::Submit});
                const vsl::operation::ScopedPresentationNativeCalls presentations({.context = &probe, .present = CloseNativeProbe::Present, .release = CloseNativeProbe::Release});
                EXPECT_EQ(native.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
                EXPECT_TRUE(callback);
                EXPECT_EQ(probe.nested, 0u);
                phase.reset();
                runtime.reset();
                EXPECT_EQ(vsl::lifetime::collect_surface_retirements(engine->data()->logical_device, vsl::lifetime::SurfaceRecoveryMode::WAIT),
                          vsl::lifetime::SurfaceRecoveryStatus::COMPLETE);
                EXPECT_EQ(result.status(), vsl::Completion::Status::CANCELLED);
                EXPECT_TRUE(generation.expired());
                EXPECT_TRUE(synchronization.expired());
                EXPECT_TRUE(b.close());
            }
        }
    }
    TEST_F(AHOWindowVulkanIntegration, CloseDuringAcquisitionResetSubmissionAndPresentationKeepsOriginalParentsUntilNativeReturn) {
        ASSERT_NO_THROW(start_engine());
        for (unsigned boundary = 0; boundary < 4; ++boundary) {
            SCOPED_TRACE(boundary);
            aho::Window window("closing during Native", 64, 48, *engine);
            CloseTargetEngine runtime(*engine, window);
            const auto drawing = window.data2;
            const std::weak_ptr<void> generation = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(drawing->swapchain).share();
            const std::weak_ptr<void> frames = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(drawing->image_available).share();
            CloseNativeProbe probe;
            bool called = false;
            unsigned finishes = 0;
            auto close = [&] {
                called = true;
                EXPECT_TRUE(window.close());
                expect_closed_resources(drawing);
                EXPECT_FALSE(generation.expired());
                EXPECT_FALSE(frames.expired());
                EXPECT_EQ(probe.nested, 0u);
            };
            const std::array callbacks{&probe.on_reset, &probe.on_acquire, &probe.on_submit, &probe.on_present};
            *callbacks[boundary] = close;
            const vsl::operation::ScopedSubmissionNativeCalls submissions({.context = &probe, .wait_idle = CloseNativeProbe::Idle, .queue_submit = CloseNativeProbe::Submit});
            const vsl::operation::ScopedPresentationNativeCalls presentations({.context = &probe, .present = CloseNativeProbe::Present, .acquire = CloseNativeProbe::Acquire, .release = CloseNativeProbe::Release});
            const vsl::operation::ScopedSynchronizationNativeCalls synchronization({.context = &probe, .reset_fences = CloseNativeProbe::Reset});
            {
                aho::DrawPhase phase(runtime, nullptr, [&](auto& stream) {
                    ++finishes;
                    stream << vsl::command::RenderPassEnd{};
                });
                if (phase.recording())
                    (void)phase.submit();
                EXPECT_TRUE(called);
                EXPECT_FALSE(phase.recording());
            }
            EXPECT_EQ(probe.nested, 0u);
            EXPECT_EQ(finishes, boundary < 2 ? 0u : 1u);
            EXPECT_EQ(vsl::lifetime::collect_surface_retirements(engine->data()->logical_device, vsl::lifetime::SurfaceRecoveryMode::WAIT),
                      vsl::lifetime::SurfaceRecoveryStatus::COMPLETE);
            EXPECT_TRUE(generation.expired());
            EXPECT_TRUE(frames.expired());
        }
    }

    TEST_F(AHOWindowVulkanIntegration, NonpositiveLogicalResizeIsRejectedBeforeAnyWindowOrGenerationChanges) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto size = window.window_size();
        const auto chain = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain);
        aho_test::AcquisitionCalls native;
        for (const auto [width, height] : {std::pair{0, 64}, std::pair{64, 0}, std::pair{-1, 64}, std::pair{64, -1}}) {
            EXPECT_THROW(window.resize(width, height), std::invalid_argument);
            EXPECT_THROW(window.VSL_NAMESPACE::PureWindow::resize(width, height), std::invalid_argument);
        }
        const auto current = window.window_size();
        EXPECT_EQ(current.value.x.value, size.value.x.value);
        EXPECT_EQ(current.value.y.value, size.value.y.value);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), chain);
        EXPECT_FALSE(window.data2->pending_generation);
        EXPECT_FALSE(window.data2->deferred_usage);
        EXPECT_EQ(native.creates, 0u);
    }
    TEST_F(AHOWindowVulkanIntegration, ZeroFramebufferKeepsValidatedUsageAndRejectsInvalidAdditionsWithoutPoisoningResume) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto previous = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain);
        aho_test::FrameBufferQuery query;
        aho_test::AcquisitionCalls native;
        query.zero = true;
        EXPECT_FALSE(window.recreate_swapchain(vsl::ImageType::TRANSFER_SRC));
        ASSERT_TRUE(window.data2->deferred_usage);
        const auto request = window.data2->deferred_usage;
        const auto invalid = static_cast<vsl::ImageType>(std::uint32_t{1} << 31);
        EXPECT_THROW(window.recreate_swapchain(invalid), std::invalid_argument);
        EXPECT_EQ(window.data2->deferred_usage, request);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), previous);
        EXPECT_EQ(window.data2->swapchain.state(), vsl::SwapchainState::ACTIVE);
        EXPECT_EQ(native.creates, 0u);
        query.zero = false;
        EXPECT_THROW(window.recreate_swapchain(invalid), std::invalid_argument);
        EXPECT_EQ(window.data2->deferred_usage, request);
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_FALSE(window.data2->deferred_usage);
        EXPECT_EQ(native.creates, 1u);
        EXPECT_TRUE(vsl::contain(window.data2->swapchain.image(0).get_usage(), vsl::ImageType::TRANSFER_SRC));
    }
    TEST_F(AHOWindowVulkanIntegration, ClosingDuringTheSecondFramebufferQueryStopsBeforeNativeCreation) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        aho_test::FrameBufferQuery query;
        aho_test::AcquisitionCalls native;
        query.callback_at = 2;
        query.after_read = [&] { EXPECT_TRUE(window.close()); };
        EXPECT_FALSE(window.recreate_swapchain());
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_EQ(query.calls, 2u);
        EXPECT_EQ(native.creates, 0u);
    }

    VkResult GenerationProbe::Create(void* context, VkDevice device, const VkSwapchainCreateInfoKHR* info, VkSwapchainKHR* output) {
        auto& probe = *static_cast<GenerationProbe*>(context);
        ++probe.creates;
        probe.previous = info->oldSwapchain;
        const auto result = vkCreateSwapchainKHR(device, info, nullptr, output);
        if (!probe.fail_create || result != VK_SUCCESS)
            return result;
        // driver側でもoldを退役させてから、生成境界の失敗を再現する。
        vkDestroySwapchainKHR(device, *output, nullptr);
        *output = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    vk::Framebuffer GenerationProbe::Frame(void* context, vk::Device device, const vk::FramebufferCreateInfo& info) {
        auto& probe = *static_cast<GenerationProbe*>(context);
        ++probe.frames;
        if (probe.fail_frame)
            throw vk::OutOfHostMemoryError("Framebuffer test failure");
        return device.createFramebuffer(info);
    }
    void GenerationProbe::DestroyFrame(void* context, vk::Device device, vk::Framebuffer frame) noexcept {
        ++static_cast<GenerationProbe*>(context)->frame_destroys;
        device.destroyFramebuffer(frame);
    }

    TEST_F(AHOWindowVulkanIntegration, FramebufferFailureKeepsTheCandidateAndRetryPublishesItWithoutCreatingAnotherChain) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto published = window.data2->swapchain;
        const auto pass = window.data2->render_pass;
        const auto frames = window.data2->frame_buffer;
        const auto available = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->image_available);
        const auto flight = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->in_flight);
        const auto acquire = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->acquire_fences);
        GenerationProbe probe;
        const vsl::operation::ScopedSwapchainNativeCalls calls({&probe, GenerationProbe::Create});
        const vsl::operation::ScopedFrameBufferNativeFactory factory({&probe, GenerationProbe::Frame, GenerationProbe::DestroyFrame});
        probe.fail_frame = true;
        EXPECT_THROW(window.recreate_swapchain(vsl::ImageType::TRANSFER_SRC), vk::OutOfHostMemoryError);
        ASSERT_TRUE(window.data2->pending_generation);
        ASSERT_TRUE(window.data2->pending_generation->swapchain);
        const auto candidate = *window.data2->pending_generation->swapchain;
        EXPECT_EQ(candidate.state(), vsl::SwapchainState::ACTIVE);
        EXPECT_EQ(published.state(), vsl::SwapchainState::RETIRED);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(published));
        EXPECT_EQ(window.data2->frame_buffer.identity(), frames.identity());
        EXPECT_EQ(probe.creates, 1u);
        unsigned notifications = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++notifications;
            EXPECT_FALSE(current->data2->pending_generation);
            EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(current->data2->swapchain), VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(candidate));
            EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(current->data2->render_finished), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(candidate.finished_semaphores()));
            EXPECT_EQ(current->data2->frame_buffer.get_render_pass().identity(), pass.identity());
            EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::GraphicsAccess::Owned(current->data2->frame_buffer)->attachments.front().front(), VSL_NAMESPACE::lifetime::detail::ImageAccess::Owned(candidate.image(0)).get());
        });
        probe.fail_frame = false;
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(notifications, 1u);
        EXPECT_EQ(window.data2->render_pass.identity(), pass.identity());
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->image_available), available);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->in_flight), flight);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->acquire_fences), acquire);
        EXPECT_TRUE(vsl::contain(window.data2->swapchain.image(0).get_usage(), vsl::ImageType::TRANSFER_SRC));
    }
    TEST_F(AHOWindowVulkanIntegration, ChangedExtentRetiresThePendingCandidateAndPublishesANewOne) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        GenerationProbe probe;
        const vsl::operation::ScopedSwapchainNativeCalls calls({&probe, GenerationProbe::Create});
        const vsl::operation::ScopedFrameBufferNativeFactory factory({&probe, GenerationProbe::Frame, GenerationProbe::DestroyFrame});
        probe.fail_frame = true;
        EXPECT_THROW(window.recreate_swapchain(), vk::OutOfHostMemoryError);
        const auto candidate = *window.data2->pending_generation->swapchain;
        probe.fail_frame = false;
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(probe.creates, 2u);
        EXPECT_EQ(probe.previous, static_cast<VkSwapchainKHR>(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(candidate)->swap_chain));
        EXPECT_EQ(candidate.state(), vsl::SwapchainState::RETIRED);
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(candidate));
    }
    TEST_F(AHOWindowVulkanIntegration, NativeFailureRetainsTheValidatedRequestAndInvalidAdditionalUsageDoesNotPoisonRetry) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        GenerationProbe probe;
        const vsl::operation::ScopedSwapchainNativeCalls calls({&probe, GenerationProbe::Create});
        probe.fail_create = true;
        EXPECT_THROW(window.recreate_swapchain(vsl::ImageType::TRANSFER_SRC), vsl::exceptions::RuntimeException);
        ASSERT_TRUE(window.data2->pending_generation);
        EXPECT_FALSE(window.data2->pending_generation->swapchain);
        EXPECT_EQ(window.data2->swapchain.state(), vsl::SwapchainState::RETIRED);
        const auto invalid = static_cast<vsl::ImageType>(std::uint32_t{1} << 31);
        EXPECT_THROW(window.recreate_swapchain(invalid), std::invalid_argument);
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_FALSE(vsl::contain(window.data2->pending_generation->request.usage, invalid));
        probe.fail_create = false;
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(probe.creates, 2u);
        EXPECT_EQ(probe.previous, VK_NULL_HANDLE);
        EXPECT_TRUE(vsl::contain(window.data2->swapchain.image(0).get_usage(), vsl::ImageType::TRANSFER_SRC));
    }
    TEST_F(AHOWindowVulkanIntegration, InvalidAdditionalUsageLeavesAnActivePendingCandidateAvailableForRetry) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        GenerationProbe probe;
        const vsl::operation::ScopedSwapchainNativeCalls calls({&probe, GenerationProbe::Create});
        const vsl::operation::ScopedFrameBufferNativeFactory factory({&probe, GenerationProbe::Frame, GenerationProbe::DestroyFrame});
        probe.fail_frame = true;
        EXPECT_THROW(window.recreate_swapchain(), vk::OutOfHostMemoryError);
        const auto candidate = *window.data2->pending_generation->swapchain;
        const auto invalid = static_cast<vsl::ImageType>(std::uint32_t{1} << 31);
        EXPECT_THROW(window.recreate_swapchain(invalid), std::invalid_argument);
        EXPECT_EQ(candidate.state(), vsl::SwapchainState::ACTIVE);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(*(window.data2->pending_generation->swapchain)), VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(candidate));
        probe.fail_frame = false;
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(candidate));
    }
    bool expect_recreation_guard(aho::Window& window) {
        // ガードが欠けても、VSLのusage検証で止めて記録中のswapchainを破棄しない。
        const auto unsupported = static_cast<vsl::ImageType>(std::uint32_t{1} << 31);
        try {
            window.recreate_swapchain(unsupported);
            ADD_FAILURE() << "Recording or reentrant recreation was accepted";
        } catch (const std::invalid_argument& error) {
            ADD_FAILURE() << "Recreation reached usage validation before its guard: " << error.what();
        } catch (const std::logic_error&) {
            return true;
        }
        return false;
    }

    TEST_F(AHOWindowVulkanIntegration, CompletionCallbackCanCloseTheWindowWhileRecreationWaits) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        auto manager = engine->data()->command_manager;
        auto phase = manager.start_phase<vsl::ComputePhase>();
        bool callback = false;
        manager.get_buffer().retain_until_complete(std::shared_ptr<int>(new int(0), [&](int* value) {
            delete value;
            callback = true;
            EXPECT_TRUE(window.close());
            EXPECT_FALSE(window.close());
        }));
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_FALSE(window.recreate_swapchain());
        EXPECT_TRUE(callback);
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_FALSE(window.data2);
    }
    TEST_F(AHOWindowVulkanIntegration, NativeResizeCallbackClosePreventsLaterRecreationAndResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        window.add_plugin<vsl::window_plugin::HookResize>([&](vsl::PureWindow, int, int) { EXPECT_TRUE(window.close()); });
        auto* native = static_cast<GLFWwindow*>(window.data_->window_handle);
        const auto callback = glfwSetFramebufferSizeCallback(native, nullptr);
        ASSERT_NE(callback, nullptr);
        callback(native, 80, 64);
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_FALSE(window.data2);
        EXPECT_THROW(window.recreate_swapchain(), std::logic_error);
        EXPECT_THROW(window.resize(80, 64), std::logic_error);
    }
    TEST_F(AHOWindowVulkanIntegration, ClosedWindowsRejectNativeQueriesAndOperations) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        ASSERT_TRUE(window.close());
        EXPECT_THROW(window.maximize(), vsl::exceptions::RuntimeException);
        EXPECT_THROW(window.restore(), vsl::exceptions::RuntimeException);
        EXPECT_THROW(window.set_title("closed"), vsl::exceptions::RuntimeException);
        EXPECT_THROW(window.window_size(), vsl::exceptions::RuntimeException);
        EXPECT_THROW(window.frame_size(), vsl::exceptions::RuntimeException);
    }
    TEST_F(AHOWindowVulkanIntegration, ManagedInputsAndTheirCopiesKeepTheOsWindowAfterManagerAndWindowClose) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        std::weak_ptr<void> native = window.window_owner().owner_ref().share();
        std::shared_ptr<aho::input::Cursor> cursor;
        std::optional<aho::input::KeyBoard> keyboard;
        std::optional<aho::input::MouseWheel> wheel;
        {
            aho::InputManager manager(window);
            cursor = manager.get<aho::input::Cursor>();
            const auto original_keyboard = manager.get<aho::input::KeyBoard>();
            const auto original_wheel = manager.get<aho::input::MouseWheel>();
            keyboard.emplace(*original_keyboard);
            wheel.emplace(*original_wheel);
        }
        ASSERT_TRUE(window.close());
        EXPECT_FALSE(native.expired());
        EXPECT_NO_THROW(cursor->update());
        EXPECT_NO_THROW(keyboard->state());
        EXPECT_NO_THROW(wheel->state());
        cursor.reset();
        keyboard.reset();
        EXPECT_FALSE(native.expired());
        wheel.reset();
        EXPECT_TRUE(native.expired());
        vsl::lifetime::GLFWLease::Drain();
    }
    TEST_F(AHOWindowVulkanIntegration, RecreationNotifiesAfterReplacingResourcesWithoutResizingTheNativeWindow) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto previous_swapchain = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain);
        const auto previous_frames = window.data2->frame_buffer;
        const auto previous_pass = window.data2->render_pass;
        const auto previous_usage = window.data2->swapchain.image(0).get_usage();
        const auto size = window.window_size();
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++calls;
            EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(current->data2->swapchain), previous_swapchain);
            EXPECT_NE(current->data2->frame_buffer.identity(), previous_frames.identity());
            EXPECT_EQ(current->data2->render_pass.identity(), previous_pass.identity());
            EXPECT_EQ(current->data2->swapchain.image(0).get_usage(), previous_usage);
            EXPECT_EQ(current->data2->render_finished.size(), current->data2->swapchain.get_swap_image_size());
        });
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(window.window_size().value.x.value, size.value.x.value);
        EXPECT_EQ(window.window_size().value.y.value, size.value.y.value);
    }

    TEST_F(AHOWindowVulkanIntegration, RecordingGuardPrecedesNativeResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto size = window.window_size();
        const auto previous = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain);
        aho::DrawPhase phase(*engine);
        EXPECT_THROW(window.resize(480, 320), std::logic_error);
        EXPECT_EQ(window.window_size().value.x.value, size.value.x.value);
        EXPECT_EQ(window.window_size().value.y.value, size.value.y.value);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), previous);
        expect_recreation_guard(window);
    }

    TEST_F(AHOWindowVulkanIntegration, RecordingGuardFindsTheOuterPhaseOfAnotherEngine) {
        ASSERT_NO_THROW(start_engine());
        auto close_engine = [](aho::StandardEngine* other) {
            other->data()->command_manager.wait_idle();
            other->boot_window->close();
            delete other;
        };
        std::unique_ptr<aho::StandardEngine, decltype(close_engine)> other(new aho::StandardEngine("nested recreation"), close_engine);
        auto& window = *engine->boot_window;
        const auto previous = VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain);
        aho::DrawPhase outer(*engine);
        {
            aho::DrawPhase inner(*other);
            expect_recreation_guard(window);
            EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &inner);
            EXPECT_EQ(outer.state, vsl::ComputePhase::State::WAIT_SUBMIT);
            EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), previous);
        }
        EXPECT_EQ(aho::THREAD_LOCAL_DRAW_PHASE, &outer);
    }

    TEST_F(AHOWindowVulkanIntegration, NotificationRejectsRecreationThroughACopiedWindow) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++calls;
            auto copy = *current;
            if (!expect_recreation_guard(copy))
                return;
            const auto size = copy.window_size();
            EXPECT_THROW(copy.resize(480, 320), std::logic_error);
            EXPECT_EQ(copy.window_size().value.x.value, size.value.x.value);
            EXPECT_EQ(copy.window_size().value.y.value, size.value.y.value);
        });
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(calls, 1);
    }

    TEST_F(AHOWindowVulkanIntegration, ThrowingNotificationAllowsAFollowingRecreation) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) {
            if (++calls == 1)
                throw std::runtime_error("resize listener failed");
        });
        EXPECT_THROW(window.recreate_swapchain(), std::runtime_error);
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(calls, 2);
    }

    TEST_F(AHOWindowVulkanIntegration, ClosedWindowRejectsRecreationAndResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        EXPECT_TRUE(window.close());
        EXPECT_THROW(window.recreate_swapchain(), std::logic_error);
        EXPECT_THROW(window.resize(480, 320), std::logic_error);
    }

    TEST_F(AHOWindowVulkanIntegration, ResizeHookCanRemoveItselfAndIsReleasedBeforeTheNextHook) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int self_calls = 0, follower_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> self;
        std::weak_ptr<aho::window::WindowResizeHookPlugin> weak;
        self = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++self_calls;
            EXPECT_TRUE(current->remove_plugin(self));
            self.reset();
            EXPECT_FALSE(weak.expired());
        });
        weak = self;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) {
            ++follower_calls;
            EXPECT_TRUE(weak.expired());
        });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(self_calls, 1);
        EXPECT_EQ(follower_calls, 1);
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(self_calls, 1);
        EXPECT_EQ(follower_calls, 2);
    }
    TEST_F(AHOWindowVulkanIntegration, HooksAddedDuringResizeWaitUntilTheNextNotification) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int first_calls = 0, added_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> added;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            ++first_calls;
            if (!added)
                added = current->add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++added_calls; });
        });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(first_calls, 1);
        EXPECT_EQ(added_calls, 0);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(first_calls, 2);
        EXPECT_EQ(added_calls, 1);
    }
    TEST_F(AHOWindowVulkanIntegration, RemovedHooksAreSkippedEvenIfTheCallerStillOwnsThem) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int removed_calls = 0, remaining_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> removed;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            if (removed)
                current->remove_plugin(removed);
        });
        removed = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++removed_calls; });
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++remaining_calls; });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(removed_calls, 0);
        EXPECT_EQ(remaining_calls, 1);
        EXPECT_TRUE(static_cast<bool>(removed));
    }
    TEST_F(AHOWindowVulkanIntegration, ClosingTheWindowDuringAResizeStopsItsRemainingHooks) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int stale_calls = 0;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([](aho::Window* current) { EXPECT_TRUE(current->close()); });
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++stale_calls; });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(stale_calls, 0);
        EXPECT_FALSE(static_cast<bool>(window));
        EXPECT_FALSE(window.data2);
    }
    TEST_F(AHOWindowVulkanIntegration, ClearingAllPluginsDuringAResizeKeepsTheRunningHookAlive) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int stale_calls = 0;
        std::weak_ptr<aho::window::WindowResizeHookPlugin> weak;
        auto current_hook = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            current->data_->plugins.clear();
            EXPECT_FALSE(weak.expired());
        });
        weak = current_hook;
        current_hook.reset();
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++stale_calls; });
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_TRUE(weak.expired());
        EXPECT_EQ(stale_calls, 0);
        EXPECT_TRUE(window.data_->plugins.empty());
        EXPECT_TRUE(window.resize(400, 400));
    }
    TEST_F(AHOWindowVulkanIntegration, ReplacementAtTheSameIdWaitsUntilTheNextResize) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int old_calls = 0, replacement_calls = 0;
        std::shared_ptr<aho::window::WindowResizeHookPlugin> old;
        std::shared_ptr<FixedIdResizeHook> replacement;
        window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* current) {
            if (!replacement)
                replacement = current->add_plugin<FixedIdResizeHook>(old->id, [&](aho::Window*) { ++replacement_calls; });
        });
        old = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window*) { ++old_calls; });
        ASSERT_NE(old->id, 0u);
        EXPECT_TRUE(window.resize(420, 300));
        EXPECT_EQ(old_calls, 0);
        EXPECT_EQ(replacement_calls, 0);
        EXPECT_TRUE(window.resize(400, 400));
        EXPECT_EQ(old_calls, 0);
        EXPECT_EQ(replacement_calls, 1);
    }
    TEST_F(AHOWindowVulkanIntegration, NativeGenerationFailureRetiresTheOldChainAndRecreationRetriesWithoutIt) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto previous = window.data2->swapchain;
        const vsl::lifetime::SwapchainMemoryFactory fail = [](const vsl::storage::SwapchainLayout&) -> vsl::storage::ArenaMemory {
            throw std::bad_alloc{};
        };
        EXPECT_THROW((vsl::lifetime::SwapchainArena{engine->data()->logical_device, *window.data2->surface, {}, previous, fail}), std::bad_alloc);
        EXPECT_EQ(window.data2->swapchain.state(), vsl::SwapchainState::RETIRED);
        EXPECT_TRUE(static_cast<bool>(window));
        EXPECT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(window.data2->swapchain.state(), vsl::SwapchainState::ACTIVE);
        EXPECT_EQ(previous.state(), vsl::SwapchainState::RETIRED);
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(window.data2->swapchain), VSL_NAMESPACE::lifetime::detail::SwapchainAccess::Owned(previous));
    }
    TEST_F(AHOWindowVulkanIntegration, GraphicsInitializationFailureUnregistersTheNewNativeWindow) {
        ASSERT_NO_THROW(start_engine());
        aho::engine::EngineAccessor failing(std::make_shared<aho::engine::EngineData>(*engine->data()));
        vsl::InstanceCreateInfo info;
        info.extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __APPLE__
        info.extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        const auto unsupported = vsl::VulkanAccessor::Create(std::move(info));
        failing.data()->vulkan_instance.accessor = unsupported.accessor;
        failing.data()->vulkan_instance.data_ = unsupported.accessor;
        const auto previous = vsl::PureWindow::WINDOWS;
        const auto previous_default = aho::engine::DefaultEngineRegistration::Current().data();
        EXPECT_THROW((aho::Window("cannot create graphics", 48, 32, failing)), vsl::exceptions::RuntimeException);
        EXPECT_EQ(vsl::PureWindow::WINDOWS, previous);
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), previous_default);
        EXPECT_TRUE(static_cast<bool>(*engine->boot_window));
        aho::Window retry("valid construction after failure", 48, 32, *engine);
        EXPECT_TRUE(static_cast<bool>(retry));
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), previous.size() + 1);
        EXPECT_TRUE(retry.close());
    }
    TEST_F(AHOWindowVulkanIntegration, BootedDefaultEngineCreatesAndClosesAnIndependentWindow) {
        ASSERT_NO_THROW(start_engine());
        const auto previous = vsl::PureWindow::WINDOWS.size();
        aho::Window second("default engine construction", 48, 32);
        EXPECT_EQ(second.engine.data(), engine->data());
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), previous + 1);
        EXPECT_TRUE(second.close());
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), previous);
    }
    TEST_F(AHOWindowVulkanIntegration, ExistingWindowSupportsSizeTitleAndClose) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        EXPECT_TRUE(window.set_title("AHO changed title"));
        auto* native = static_cast<GLFWwindow*>(window.data_->window_handle);
        EXPECT_STREQ(glfwGetWindowTitle(native), "AHO changed title");
        window.maximize();
        glfwPollEvents();
        EXPECT_EQ(glfwGetWindowAttrib(native, GLFW_MAXIMIZED), GLFW_TRUE);
        window.restore();
        glfwPollEvents();
        EXPECT_EQ(glfwGetWindowAttrib(native, GLFW_MAXIMIZED), GLFW_FALSE);
        EXPECT_TRUE(window.close());
        EXPECT_EQ(window.data2, nullptr);
        EXPECT_EQ(window.data_->window_handle, nullptr);
    }
    TEST_F(AHOWindowVulkanIntegration, MonitorSizeRequiresAvailablePrimaryDisplayMode) {
        auto* monitor = glfwGetPrimaryMonitor();
        if (!monitor) {
            EXPECT_THROW(aho::window::get_monitor_size(), vsl::exceptions::RuntimeException);
            return;
        }
#ifdef __APPLE__
        const auto display = glfwGetCocoaMonitor(monitor);
        auto mode = CGDisplayCopyDisplayMode(display);
        if (!mode) {
            EXPECT_THROW(aho::window::get_monitor_size(), vsl::exceptions::RuntimeException);
            return;
        }
        CGDisplayModeRelease(mode);
#endif
        const auto* mode_info = glfwGetVideoMode(monitor);
        ASSERT_NE(mode_info, nullptr) << "GLFW failed to query a video mode exposed by the OS.";
        const auto size = aho::window::get_monitor_size();
        EXPECT_EQ(size.value.x.value, mode_info->width);
        EXPECT_EQ(size.value.y.value, mode_info->height);
        EXPECT_GT(size.value.x.value, 0);
        EXPECT_GT(size.value.y.value, 0);
    }
    TEST_F(AHOWindowVulkanIntegration, ResizingRecreatesFrameResourcesAndCallsHook) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        int callbacks = 0;
        auto hook = window.add_plugin<aho::window::WindowResizeHookPlugin>([&](aho::Window* received) {
            EXPECT_EQ(received, &window);
            ++callbacks;
        });
        EXPECT_TRUE(window.resize(480, 320));
        EXPECT_EQ(callbacks, 1);
        EXPECT_EQ(window.window_size().value.x.value, 480);
        EXPECT_EQ(window.window_size().value.y.value, 320);
        EXPECT_TRUE(window.resize(aho::d2::VectorI(400, 400)));
        EXPECT_EQ(callbacks, 2);
        EXPECT_EQ(window.window_size().value.x.value, 400);
        EXPECT_EQ(window.window_size().value.y.value, 400);
    }
    TEST_F(AHOWindowVulkanIntegration, ResizeReplacesFinishedSemaphoresEvenWhenTheImageCountIsUnchanged) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto old = window.data2->render_finished;
        const auto count = old.size();
        ASSERT_TRUE(window.resize(320, 240));
        const auto current = window.data2->render_finished;
        EXPECT_EQ(current.size(), count);
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(current), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->swapchain.finished_semaphores()));
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(current), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(old));
        EXPECT_TRUE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(current).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(old)) || VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(old).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(current)));
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(current)->semaphores[0], VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(old)->semaphores[0]);
    }
    TEST_F(AHOWindowVulkanIntegration, ExplicitEngineConstructorCreatesIndependentWindow) {
        ASSERT_NO_THROW(start_engine());
        const std::string name = "AHO secondary synchronization resources";
        aho::Window second(name, 240, 160, *engine);
        EXPECT_NE(second.data_, engine->boot_window->data_);
        EXPECT_EQ(second.engine.data(), engine->data());
        EXPECT_EQ(second.window_size().value.x.value, 240);
        EXPECT_EQ(second.window_size().value.y.value, 160);
        EXPECT_EQ(second.name(), name);
        auto available = engine->data()->synchro_manager.get_semaphore(name + "ImageAvailable");
        auto finished = engine->data()->synchro_manager.get_semaphore(name + "RenderFinished");
        auto in_flight = engine->data()->synchro_manager.get_fence(name + "InFlight");
        EXPECT_FALSE(available.has_value());
        EXPECT_FALSE(finished.has_value());
        EXPECT_FALSE(in_flight.has_value());
        EXPECT_EQ(second.data2->image_available.size(), engine->data()->command_manager.get_buffer().get_size());
        EXPECT_EQ(second.data2->in_flight.size(), second.data2->image_available.size());
        EXPECT_EQ(second.data2->acquire_fences.size(), second.data2->image_available.size());
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(second.data2->render_finished), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(second.data2->swapchain.finished_semaphores()));
        EXPECT_TRUE(second.close());
    }
    TEST_F(AHOWindowVulkanIntegration, WindowFrameSynchronizationSharesOneRootAndIsReleasedBeforeTheEngine) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto& resources = *window.data2;
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.in_flight)));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.in_flight).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available)));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.acquire_fences)));
        EXPECT_FALSE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.acquire_fences).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available)));
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.in_flight), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.acquire_fences));
        EXPECT_TRUE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.render_finished)) ||
                    VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.render_finished).owner_before(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available)));
        const std::weak_ptr<void> owner = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(resources.image_available).share();
        EXPECT_TRUE(window.close());
        EXPECT_TRUE(engine->data());
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(AHOWindowVulkanIntegration, RecreationKeepsFrameSynchronizationAndReplacesTheGenerationSynchronization) {
        ASSERT_NO_THROW(start_engine());
        auto& window = *engine->boot_window;
        const auto available = window.data2->image_available;
        const auto in_flight = window.data2->in_flight;
        const auto acquired = window.data2->acquire_fences;
        const auto finished = window.data2->render_finished;
        ASSERT_TRUE(window.recreate_swapchain());
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->image_available), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(available));
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->in_flight), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(in_flight));
        EXPECT_EQ(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->acquire_fences), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(acquired));
        EXPECT_NE(VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(window.data2->render_finished), VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(finished));
        EXPECT_EQ(window.data2->render_finished.size(), window.data2->swapchain.get_swap_image_size());
    }
    TEST_F(AHOWindowVulkanIntegration, EquallyNamedWindowsHaveIndependentFrameSynchronization) {
        ASSERT_NO_THROW(start_engine());
        aho::Window first("shared title", 64, 48, *engine);
        aho::Window second("shared title", 64, 48, *engine);
        const auto& a = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(first.data2->image_available);
        const auto& b = VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Owned(second.data2->image_available);
        EXPECT_TRUE(a.owner_before(b) || b.owner_before(a));
        const std::weak_ptr<void> owner = a.share();
        EXPECT_TRUE(first.close());
        EXPECT_TRUE(owner.expired());
        EXPECT_TRUE(second.data2->in_flight.wait(std::uint64_t{0}));
        EXPECT_TRUE(second.close());
    }
    TEST_F(AHOWindowVulkanIntegration, WindowSynchronizationDoesNotRequireAnEngineNamedRegistry) {
        ASSERT_NO_THROW(start_engine());
        aho::engine::EngineAccessor runtime(std::make_shared<aho::engine::EngineData>(*engine->data()));
        VSL_NAMESPACE::lifetime::detail::SynchronizationAccess::Registry(runtime.data()->synchro_manager).reset();
        aho::Window independent("without a name registry", 64, 48, runtime);
        EXPECT_EQ(independent.data2->image_available.size(), runtime.data()->command_manager.get_buffer().get_size());
        EXPECT_TRUE(independent.data2->acquire_fences.owner_ref().managed());
        EXPECT_TRUE(independent.close());
    }
    TEST_F(AHOWindowVulkanIntegration, AnExplicitStrictWindowRequiresTheFeatureToHaveBeenEnabled) {
        ASSERT_NO_THROW(start_engine());
        EXPECT_EQ(engine->data()->present_fences, vsl::PresentFencePolicy::LEGACY);
        const auto windows = vsl::PureWindow::WINDOWS.size();
        EXPECT_THROW((aho::Window("requires feature", 64, 48, *engine, vsl::PresentFencePolicy::REQUIRE_FENCE)), std::invalid_argument);
        EXPECT_EQ(vsl::PureWindow::WINDOWS.size(), windows);
    }
} // namespace
#else
#include <gtest/gtest.h>
#include <AHO/window.hpp>
#include <AHO/engine/standard_engine.hpp>
namespace {
    TEST(AHOWindowConstructionTest, MissingDefaultEngineThrowsBeforeWindowRegistration) {
        ASSERT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_THROW((aho::Window("no default engine")), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
    }
    TEST(AHOWindowConstructionTest, UnbootedDefaultEngineThrowsBeforeWindowRegistration) {
        aho::StandardEngine engine("not booted", false);
        ASSERT_FALSE(engine.data());
        EXPECT_THROW((aho::Window("unbooted default engine")), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
    }
    TEST(AHOWindowConstructionTest, UnbootedExplicitEngineThrowsBeforeWindowRegistration) {
        aho::StandardEngine engine("not booted", false);
        ASSERT_FALSE(engine.data());
        EXPECT_THROW((aho::Window("unbooted explicit engine", 48, 32, engine)), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(vsl::PureWindow::WINDOWS.empty());
        EXPECT_EQ(aho::engine::DefaultEngineRegistration::Current().data(), nullptr);
    }
    TEST(AHOWindowTest, SharedDataConstructorKeepsNativeOwnershipAndEmptyDrawingData) {
        aho::engine::EngineAccessor engine;
        auto native = std::make_shared<vsl::Window::WindowData>();
        std::shared_ptr<aho::window::WindowData> drawing;
        aho::Window window(engine, native, drawing);
        EXPECT_EQ(window.data_, native);
        EXPECT_EQ(window.data2, drawing);
        EXPECT_EQ(window.engine.data(), engine.data());
    }
    TEST(WindowResizeHookTest, ConstructorStoresCallableAndPassesWindowPointer) {
        aho::Window* received = nullptr;
        aho::window::WindowResizeHookPlugin plugin(nullptr, [&](aho::Window* window) { received = window; });
        EXPECT_TRUE(static_cast<bool>(plugin.hook));
        plugin.hook(nullptr);
        EXPECT_EQ(received, nullptr);
    }
} // namespace
#endif

#ifndef AHO_GPU_TESTS
#include <gtest/gtest.h>
#include <AHO/window.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#include <GLFW/glfw3.h>
namespace {
    TEST(MonitorSizeTest, UninitializedGlfwThrowsInsteadOfDereferencingANullMode) {
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        vsl::lifetime::GLFWLease::Drain();
        glfwTerminate();
        try {
            (void)aho::window::get_monitor_size();
            ADD_FAILURE() << "Missing primary monitor was accepted";
        } catch (const vsl::exceptions::RuntimeException& error) {
            EXPECT_NE(std::string(error.what()).find("monitor"), std::string::npos);
        }
        glfwGetError(nullptr);
    }
#if defined(GLFW_PLATFORM_NULL)
    TEST(MonitorSizeTest, InitializedNullPlatformReturnsItsVirtualMonitorSize) {
        ASSERT_TRUE(vsl::PureWindow::WINDOWS.empty());
        vsl::lifetime::GLFWLease::Drain();
        struct RestorePlatform {
            ~RestorePlatform() {
                glfwTerminate();
                glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
            }
        } restore;
        glfwTerminate();
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
        ASSERT_TRUE(glfwInit());
        ASSERT_NE(glfwGetPrimaryMonitor(), nullptr);
        const auto size = aho::window::get_monitor_size();
        EXPECT_EQ(size.value.x.value, 1920);
        EXPECT_EQ(size.value.y.value, 1080);
        EXPECT_EQ(glfwGetError(nullptr), GLFW_NO_ERROR);
    }
#endif
} // namespace

#endif // AHO_GPU_TESTS
