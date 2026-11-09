#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include "support/window_fixture.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/surface_recovery.hpp>
#include <VSL/lifetime/acquire_ticket.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/lifetime/startup_arena.hpp>
#include <VSL/operation/command_ops.hpp>
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/sync/submission_completion.hpp>
#include <functional>

namespace {
    using VSL_NAMESPACE::lifetime::detail::SynchronizationAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace l = vsl::lifetime;
    namespace s = vsl::sync;
    namespace o = vsl::operation;
    class SurfaceRecoveryWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
        vsl::PresentFencePolicy present_fence_policy() const override { return vsl::PresentFencePolicy::REQUIRE_FENCE; }
        void SetUp() override;

    protected:
        vsl::SwapchainAccessor make_chain(vsl::PresentFencePolicy policy = vsl::PresentFencePolicy::REQUIRE_FENCE);
    };
    struct RecoveryProbe {
        VkResult graphics_error = VK_SUCCESS, drain_error = VK_SUCCESS, present_error = VK_SUCCESS;
        unsigned graphics = 0, drains = 0, idles = 0, releases = 0, fail_idle = 0;
        std::function<void()> on_idle, on_present, on_acquire, on_wait, on_graphics;
        static VkResult BeginFailure(void*, VkCommandBuffer, const VkCommandBufferBeginInfo*) noexcept;
        static VkResult Submit(void*, VkQueue, const VkSubmitInfo*, VkFence) noexcept;
        static VkResult Idle(void*, VkDevice) noexcept;
        static VkResult Present(void*, VkQueue, const VkPresentInfoKHR*) noexcept;
        static VkResult Release(void*, VkDevice, const VkReleaseSwapchainImagesInfoEXT*) noexcept;
        static VkResult Acquire(void*, VkDevice, VkSwapchainKHR, std::uint64_t, VkSemaphore, VkFence, std::uint32_t*) noexcept;
        static VkResult Wait(void*, VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t) noexcept;
        static VkResult Status(void*, VkDevice, VkFence) noexcept;
        o::SubmissionNativeCalls submissions() { return {.context = this, .wait_idle = Idle, .wait_fences = Wait, .queue_submit = Submit, .fence_status = Status}; }
        o::PresentationNativeCalls presentations() { return {.context = this, .present = Present, .acquire = Acquire, .release = Release}; }
    };
    void SurfaceRecoveryWindowIntegrationTest::SetUp() {
        WindowIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        make_surface();
        if (!HasFatalFailure() && !physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected queue cannot present";
    }
    vsl::SwapchainAccessor SurfaceRecoveryWindowIntegrationTest::make_chain(vsl::PresentFencePolicy policy) {
        return l::SwapchainArena(device, *surface, {.width = 64, .height = 48, .present_fences = policy}).view();
    }
    VkResult RecoveryProbe::BeginFailure(void*, VkCommandBuffer, const VkCommandBufferBeginInfo*) noexcept {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    VkResult RecoveryProbe::Submit(void* context, VkQueue queue, const VkSubmitInfo* info, VkFence fence) noexcept {
        auto& probe = *static_cast<RecoveryProbe*>(context);
        const bool drain = !info->commandBufferCount && info->waitSemaphoreCount;
        ++(drain ? probe.drains : probe.graphics);
        const auto error = drain ? probe.drain_error : probe.graphics_error;
        if (error == VK_ERROR_OUT_OF_HOST_MEMORY || error == VK_ERROR_OUT_OF_DEVICE_MEMORY)
            return error;
        const auto result = vkQueueSubmit(queue, 1, info, fence);
        if (!drain)
            if (auto callback = std::exchange(probe.on_graphics, {}))
                callback();
        if (result != VK_SUCCESS || error == VK_SUCCESS)
            return result;
        // 不明結果の注入前に実GPUの使用は完了させる。
        const auto idle = vkQueueWaitIdle(queue);
        return idle == VK_SUCCESS ? error : idle;
    }
    VkResult RecoveryProbe::Idle(void* context, VkDevice device) noexcept {
        auto& probe = *static_cast<RecoveryProbe*>(context);
        ++probe.idles;
        if (auto callback = std::exchange(probe.on_idle, {}))
            callback();
        const auto result = vkDeviceWaitIdle(device);
        return result == VK_SUCCESS && probe.idles == probe.fail_idle ? VK_ERROR_OUT_OF_HOST_MEMORY : result;
    }
    VkResult RecoveryProbe::Present(void* context, VkQueue queue, const VkPresentInfoKHR* info) noexcept {
        auto& probe = *static_cast<RecoveryProbe*>(context);
        if (probe.present_error != VK_SUCCESS)
            return probe.present_error;
        const auto result = vkQueuePresentKHR(queue, info);
        if (auto callback = std::exchange(probe.on_present, {}))
            callback();
        return result;
    }
    VkResult RecoveryProbe::Release(void* context, VkDevice device, const VkReleaseSwapchainImagesInfoEXT* info) noexcept {
        ++static_cast<RecoveryProbe*>(context)->releases;
        const auto release = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(vkGetDeviceProcAddr(device, "vkReleaseSwapchainImagesEXT"));
        return release ? release(device, info) : VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkResult RecoveryProbe::Acquire(void* context, VkDevice device, VkSwapchainKHR chain, std::uint64_t timeout, VkSemaphore available,
                                    VkFence fence, std::uint32_t* image) noexcept {
        const auto result = vkAcquireNextImageKHR(device, chain, timeout, available, fence, image);
        auto& probe = *static_cast<RecoveryProbe*>(context);
        if (auto callback = std::exchange(probe.on_acquire, {}))
            callback();
        return result;
    }
    VkResult RecoveryProbe::Wait(void* context, VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) noexcept {
        auto& probe = *static_cast<RecoveryProbe*>(context);
        if (auto callback = std::exchange(probe.on_wait, {}))
            callback();
        return vkWaitForFences(device, count, fences, all, timeout);
    }
    VkResult RecoveryProbe::Status(void* context, VkDevice device, VkFence fence) noexcept {
        return static_cast<RecoveryProbe*>(context)->on_wait ? VK_NOT_READY : vkGetFenceStatus(device, fence);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, AnUnusedSurfaceRecoversWithoutCreatingAPresentationDomain) {
        EXPECT_EQ(l::recover_surface(device, *surface, l::SurfaceRecoveryMode::POLL), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_FALSE(surface->data_->owner_context->presentations.lock());
        EXPECT_THROW(l::recover_surface(device, *surface, static_cast<l::SurfaceRecoveryMode>(99)), std::invalid_argument);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, RetirementRegistrationIsWeakAndRepeatedRegistrationUsesOneCollection) {
        RecoveryProbe probe;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        l::queue_surface_retirement(device, *surface);
        l::queue_surface_retirement(device, *surface);
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.idles, 1u);
        EXPECT_FALSE(surface->data_->owner_context->presentations.lock());
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.idles, 1u);
        l::queue_surface_retirement(device, *surface);
        const std::weak_ptr<void> owner = surface->data_;
        window->data_->plugins.clear();
        surface.reset();
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.idles, 1u);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, RegistrationDuringCollectionIsDeferredWithoutLosingTheNewRequest) {
        RecoveryProbe probe;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        l::queue_surface_retirement(device, *surface);
        probe.on_idle = [&] {
            l::queue_surface_retirement(device, *surface);
            EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::PENDING);
            EXPECT_EQ(probe.idles, 1u);
        };
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::PENDING);
        EXPECT_EQ(probe.idles, 1u);
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.idles, 2u);
        EXPECT_THROW(l::collect_surface_retirements(device, static_cast<l::SurfaceRecoveryMode>(99)), std::invalid_argument);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, FailedRetirementCollectionPreservesTheRequestForRetry) {
        RecoveryProbe probe;
        probe.fail_idle = 1;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        l::queue_surface_retirement(device, *surface);
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::PENDING);
        EXPECT_EQ(probe.idles, 1u);
        EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.idles, 2u);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, DifferentSurfaceSlotsInOneArenaAreSeparateRetirementRequests) {
        vsl::PureWindow a("same arena A", 64, 48), b("same arena B", 64, 48);
        const std::array requests{vsl::storage::StartupRequest{{1}, vsl::storage::StartupKind::SURFACE},
                                  vsl::storage::StartupRequest{{2}, vsl::storage::StartupKind::SURFACE}};
        const vsl::storage::StartupPlan<2> plan(requests);
        const std::array<l::StartupInit, 2> inputs{l::SurfaceInit{vsl::VulkanAccessor{instance}, a.window_owner()},
                                                   l::SurfaceInit{vsl::VulkanAccessor{instance}, b.window_owner()}};
        {
            const l::StartupArena arena(plan, inputs);
            const auto first = arena.surface({1}), second = arena.surface({2});
            EXPECT_FALSE(first.data_.owner_before(second.data_));
            EXPECT_FALSE(second.data_.owner_before(first.data_));
            EXPECT_NE(first.data_, second.data_);
            RecoveryProbe probe;
            const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
            l::queue_surface_retirement(device, first);
            l::queue_surface_retirement(device, second);
            l::queue_surface_retirement(device, first);
            EXPECT_EQ(l::collect_surface_retirements(device, l::SurfaceRecoveryMode::WAIT), l::SurfaceRecoveryStatus::COMPLETE);
            EXPECT_EQ(probe.idles, 2u);
        }
        EXPECT_TRUE(a.close());
        EXPECT_TRUE(b.close());
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, BeginFailureKeepsAnUnconsumedAcquireUntilTheDrainCanBeRetried) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        vsl::SynchroManager synchronization(device);
        auto available = synchronization.create_semaphore("available");
        auto phase = std::make_unique<vsl::DefaultPhase>(manager, chain, available, std::nullopt, std::nullopt, true);
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.drain_error = VK_ERROR_OUT_OF_HOST_MEMORY;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        {
            const o::ScopedCommandNativeCalls commands({.begin = RecoveryProbe::BeginFailure});
            EXPECT_THROW(phase->setup(), vk::OutOfHostMemoryError);
        }
        ASSERT_EQ(registry->tickets.size(), 1u);
        const auto ticket = registry->tickets.front();
        EXPECT_TRUE(ticket->state.acquired);
        EXPECT_TRUE(ticket->state.abandon_requested);
        EXPECT_FALSE(ticket->released);
        EXPECT_EQ(chain.state(), vsl::SwapchainState::RECOVERY_REQUIRED);
        EXPECT_EQ(phase->completion().status(), vsl::Completion::Status::FAILED);
        EXPECT_THROW(s::require_acquire_signal(*SynchronizationAccess::Owned(available), 0), std::logic_error);
        EXPECT_EQ(probe.drains, 1u);
        probe.drain_error = VK_SUCCESS;
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_TRUE(ticket->released);
        EXPECT_EQ(ticket->state.status, s::AcquisitionStatus::RETURNED);
        EXPECT_EQ(probe.drains, 2u);
        EXPECT_EQ(probe.releases, 1u);
        EXPECT_NO_THROW(s::require_acquire_signal(*SynchronizationAccess::Owned(available), 0));
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.releases, 1u);
        EXPECT_EQ(chain.state(), vsl::SwapchainState::RECOVERY_REQUIRED);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, CancelDuringNativeAcquireAdoptsTheImageBeforeRecoveringIt) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        vsl::DefaultPhase phase(manager, chain, std::nullopt, std::nullopt, std::nullopt, true);
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.on_acquire = [&] {
            EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::PENDING);
            EXPECT_EQ(probe.idles, 0u);
            phase.cancel();
        };
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        EXPECT_NO_THROW(phase.setup());
        EXPECT_EQ(phase.completion().status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(registry->tickets.empty());
        EXPECT_EQ(probe.releases, 1u);
        EXPECT_EQ(chain.state(), vsl::SwapchainState::RECOVERY_REQUIRED);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, SurfaceRecoveryReclaimsAnOrphanWithoutCreatingANewDomain) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        vsl::SynchroManager synchronization(device);
        auto available = synchronization.create_semaphore("available");
        auto phase = std::make_unique<vsl::DefaultPhase>(manager, chain, available, std::nullopt, std::nullopt, true);
        std::weak_ptr<l::SurfacePresentationDomain> registry;
        RecoveryProbe probe;
        probe.fail_idle = 1;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        std::weak_ptr<l::AcquireTicket> ticket;
        std::weak_ptr<vsl::impl::FenceImpl> fences;
        std::weak_ptr<vsl::impl::SwapchainImpl> generation = chain.data_;
        {
            const o::ScopedCommandNativeCalls commands({.begin = RecoveryProbe::BeginFailure});
            EXPECT_THROW(phase->setup(), vk::OutOfHostMemoryError);
            registry = l::CommandAccess::Owner(manager.get_buffer())->presentation_domains.front();
            ticket = registry.lock()->tickets.front();
            fences = ticket.lock()->acquire_fence_owner();
        }
        probe.fail_idle = 0;
        probe.drain_error = VK_ERROR_OUT_OF_HOST_MEMORY;
        phase.reset();
        const auto before = l::quarantined_acquisition_count();
        chain = {};
        manager = vsl::CommandManager(CommandAccess::RawManager(nullptr));
        EXPECT_TRUE(registry.expired());
        EXPECT_EQ(l::quarantined_acquisition_count(), before + 1);
        EXPECT_FALSE(ticket.expired());
        EXPECT_FALSE(fences.expired());
        EXPECT_FALSE(generation.expired());
        probe.drain_error = VK_SUCCESS;
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_TRUE(ticket.expired());
        EXPECT_TRUE(fences.expired());
        EXPECT_TRUE(generation.expired());
        EXPECT_EQ(l::quarantined_acquisition_count(), before);
        EXPECT_TRUE(surface->data_->owner_context->presentations.expired());
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, CancelDuringNativeGraphicsDoesNotPresentAnAlreadyRecoveredImage) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
        const auto original = phase.completion();
        const auto ticket = registry->tickets.front();
        probe.on_graphics = [&] { phase.cancel(); };
        probe.on_present = [&] { ADD_FAILURE() << "Recovered image cannot be presented"; };
        EXPECT_NO_THROW(phase.cancel());
        EXPECT_EQ(original.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(ticket->released);
        EXPECT_EQ(ticket->state.status, s::AcquisitionStatus::RETURNED);
        EXPECT_EQ(probe.graphics, 1u);
        EXPECT_EQ(probe.drains, 1u);
        EXPECT_EQ(probe.releases, 1u);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, AnExistingGraphicsObserverPreventsNestedDeviceIdle) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        RecoveryProbe probe;
        probe.present_error = VK_ERROR_OUT_OF_HOST_MEMORY;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
        ASSERT_EQ(phase.submit_commands(), vsl::ComputePhase::SubmitResult::SUCCESS);
        unsigned observations = 0;
        probe.on_wait = [&] {
            ++observations;
            EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::PENDING);
            EXPECT_EQ(probe.idles, 0u);
            EXPECT_THROW(manager.wait_idle(), std::logic_error);
        };
        EXPECT_TRUE(phase.completion().wait());
        EXPECT_EQ(probe.idles, 0u);
        EXPECT_EQ(observations, 1u);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, SuccessfulDrainIsNotSubmittedAgainWhenItsIdleObservationFails) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        vsl::SynchroManager synchronization(device);
        auto available = synchronization.create_semaphore("available");
        auto phase = std::make_unique<vsl::DefaultPhase>(manager, chain, available, std::nullopt, std::nullopt, true);
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.fail_idle = 2;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        {
            const o::ScopedCommandNativeCalls commands({.begin = RecoveryProbe::BeginFailure});
            EXPECT_THROW(phase->setup(), vk::OutOfHostMemoryError);
        }
        ASSERT_EQ(registry->tickets.size(), 1u);
        const auto ticket = registry->tickets.front();
        EXPECT_EQ(ticket->state.drain.enqueue, s::SubmissionEnqueue::ENQUEUED);
        EXPECT_FALSE(ticket->state.drain.complete);
        EXPECT_FALSE(ticket->released);
        EXPECT_EQ(probe.drains, 1u);
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_TRUE(ticket->released);
        EXPECT_EQ(probe.drains, 1u);
        EXPECT_EQ(probe.releases, 1u);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, UnknownDrainPoisonsItsElementAndDoesNotClaimTheImageWasReturned) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        vsl::SynchroManager synchronization(device);
        auto available = synchronization.create_semaphore("available");
        vsl::DefaultPhase phase(manager, chain, available, std::nullopt, std::nullopt, true);
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.drain_error = VK_ERROR_UNKNOWN;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        const o::ScopedCommandNativeCalls commands({.begin = RecoveryProbe::BeginFailure});
        EXPECT_THROW(phase.setup(), vk::OutOfHostMemoryError);
        EXPECT_TRUE(registry->tickets.empty());
        EXPECT_EQ(probe.drains, 1u);
        EXPECT_EQ(probe.releases, 0u);
        EXPECT_THROW(s::require_acquire_signal(*SynchronizationAccess::Owned(available), 0), std::logic_error);
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.drains, 1u);
        EXPECT_TRUE(SynchronizationAccess::Owned(available)->use.poisoned[0]);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, PresentOutOfMemoryKeepsRetryRightsUntilThePhaseIsAbandoned) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.present_error = VK_ERROR_OUT_OF_HOST_MEMORY;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        auto phase = std::make_unique<vsl::DefaultPhase>(manager, chain);
        const auto ticket = registry->tickets.front();
        EXPECT_EQ(phase->submit_commands(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_EQ(phase->present(), vsl::ComputePhase::SubmitResult::FAILED);
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::PENDING);
        EXPECT_FALSE(ticket->state.abandon_requested);
        EXPECT_EQ(probe.drains, 0u);
        phase.reset();
        EXPECT_TRUE(ticket->released);
        EXPECT_EQ(ticket->state.status, s::AcquisitionStatus::RETURNED);
        EXPECT_EQ(probe.drains, 1u);
        EXPECT_EQ(probe.releases, 1u);
        EXPECT_TRUE(registry->records.empty());
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, LegacyPresentationPolicyDoesNotDisableAnEnabledImageReleaseFeature) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain(vsl::PresentFencePolicy::LEGACY);
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.graphics_error = VK_ERROR_OUT_OF_HOST_MEMORY;
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        auto phase = std::make_unique<vsl::DefaultPhase>(manager, chain);
        const auto ticket = registry->tickets.front();
        EXPECT_THROW(phase->submit_commands(), vk::OutOfHostMemoryError);
        EXPECT_TRUE(ticket->released);
        EXPECT_EQ(ticket->state.status, s::AcquisitionStatus::RETURNED);
        EXPECT_EQ(probe.graphics, 1u);
        EXPECT_EQ(probe.releases, 1u);
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, RecoveryDefersNestedCollectionUntilEveryCapturedStateHasBeenSaved) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        const auto registry = l::presentation_domain(chain.data_);
        {
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            phase.cancel();
        }
        RecoveryProbe probe;
        probe.on_idle = [&] {
            EXPECT_TRUE(registry->recovering);
            EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::PENDING);
            l::collect_presentations(registry);
        };
        const o::ScopedSubmissionNativeCalls submissions(probe.submissions());
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::COMPLETE);
        EXPECT_EQ(probe.idles, 1u);
        EXPECT_FALSE(registry->recovering);
        EXPECT_TRUE(registry->records.empty());
    }
    TEST_F(SurfaceRecoveryWindowIntegrationTest, DeviceLossObservedInsideNativePresentDoesNotDetachTheActiveRecord) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        const auto registry = l::presentation_domain(chain.data_);
        RecoveryProbe probe;
        probe.on_present = [&] {
            // 実GPUを完了させた後にCPU側だけDeviceLostへ進める。
            ASSERT_FALSE(registry->records.empty());
            const VkFence proof = registry->records.back()->state.proof;
            ASSERT_NE(proof, VK_NULL_HANDLE);
            ASSERT_EQ(vkWaitForFences(device.data_->device, 1, &proof, VK_TRUE, UINT64_MAX), VK_SUCCESS);
            ASSERT_EQ(vkDeviceWaitIdle(device.data_->device), VK_SUCCESS);
            device.data_->lost = true;
            EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::DEVICE_LOST);
            ASSERT_FALSE(registry->records.empty());
            EXPECT_TRUE(registry->records.back()->generation);
            EXPECT_FALSE(registry->records.back()->released);
        };
        const o::ScopedPresentationNativeCalls presentations(probe.presentations());
        auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
        EXPECT_NO_THROW(phase.cancel());
        EXPECT_EQ(l::recover_surface(device, *surface), l::SurfaceRecoveryStatus::DEVICE_LOST);
        EXPECT_TRUE(registry->records.empty());
    }
} // namespace
