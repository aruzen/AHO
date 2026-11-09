#include "support/window_fixture.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/swapchain_arena.hpp>
#include <VSL/lifetime/presentation_retention.hpp>
#include <VSL/lifetime/synchronization_use.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/operation/presentation_ops.hpp>
#include <VSL/operation/submission_ops.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <new>

namespace {
    namespace l = vsl::lifetime;
    namespace s = vsl::sync;
    namespace o = vsl::operation;
    class PresentationRetentionWindowIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
        vsl::PresentFencePolicy present_fence_policy() const override { return vsl::PresentFencePolicy::REQUIRE_FENCE; }
        void SetUp() override;

    protected:
        vsl::SwapchainAccessor make_chain();
    };
    class LegacyPresentationRetentionWindowIntegrationTest : public PresentationRetentionWindowIntegrationTest {
        vsl::PresentFencePolicy present_fence_policy() const override { return vsl::PresentFencePolicy::LEGACY; }
    };
    struct ClosingWindow : vsl::PureWindow {
        using PureWindow::PureWindow;
        ~ClosingWindow() override { close(); }
    };
    struct NativeProbe {
        VkFence target = VK_NULL_HANDLE;
        VkResult forced = VK_SUCCESS;
        bool hold = true, unknown_before = false, device_lost = false;
        unsigned calls = 0, target_polls = 0, target_waits = 0;
        std::array<VkFence, 256> fences{};
        NativeProbe();
        static VkResult Present(void* context, VkQueue queue, const VkPresentInfoKHR* info) noexcept;
        static VkResult Poll(void* context, VkDevice device, VkFence fence) noexcept;
        static VkResult Wait(void* context, VkDevice device, VkFence fence, std::uint64_t timeout) noexcept;

    private:
        o::ScopedPresentationNativeCalls _scope;
    };
    vsl::storage::ArenaMemory callback_memory(const vsl::storage::SwapchainLayout& layout, std::shared_ptr<void> notify);

    vsl::storage::ArenaMemory callback_memory(const vsl::storage::SwapchainLayout& layout, std::shared_ptr<void> notify) {
        auto* bytes = static_cast<std::byte*>(::operator new(layout.bytes(), std::align_val_t(layout.alignment())));
        auto owner = std::shared_ptr<void>(bytes, [alignment = layout.alignment(), notify = std::move(notify)](void* pointer) mutable {
            ::operator delete(pointer, std::align_val_t(alignment));
            notify.reset();
        });
        return vsl::storage::ArenaMemory({bytes, layout.bytes()}, layout.alignment(), std::move(owner));
    }
    NativeProbe::NativeProbe() : _scope({this, Present, Poll, Wait}) {}
    VkResult NativeProbe::Present(void* context, VkQueue queue, const VkPresentInfoKHR* info) noexcept {
        auto& probe = *static_cast<NativeProbe*>(context);
        VkFence fence = VK_NULL_HANDLE;
        if (info->pNext) {
            const auto* value = static_cast<const VkSwapchainPresentFenceInfoEXT*>(info->pNext);
            EXPECT_EQ(value->sType, VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT);
            EXPECT_EQ(value->swapchainCount, 1u);
            fence = value->pFences[0];
        }
        if (probe.calls < probe.fences.size())
            probe.fences[probe.calls] = fence;
        ++probe.calls;
        if (probe.forced == VK_ERROR_OUT_OF_HOST_MEMORY || probe.forced == VK_ERROR_OUT_OF_DEVICE_MEMORY ||
            (probe.forced == VK_ERROR_UNKNOWN && probe.unknown_before))
            return probe.forced;
        const auto result = vkQueuePresentKHR(queue, info);
        return result == VK_SUCCESS ? probe.forced : result;
    }
    VkResult NativeProbe::Poll(void* context, VkDevice device, VkFence fence) noexcept {
        auto& probe = *static_cast<NativeProbe*>(context);
        if (fence == probe.target && fence) {
            ++probe.target_polls;
            if (probe.device_lost)
                return VK_ERROR_DEVICE_LOST;
            if (probe.hold)
                return VK_NOT_READY;
        }
        return vkGetFenceStatus(device, fence);
    }
    VkResult NativeProbe::Wait(void* context, VkDevice device, VkFence fence, std::uint64_t timeout) noexcept {
        auto& probe = *static_cast<NativeProbe*>(context);
        if (fence == probe.target && fence) {
            ++probe.target_waits;
            if (probe.hold)
                return VK_TIMEOUT;
        }
        return vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
    }
    void PresentationRetentionWindowIntegrationTest::SetUp() {
        vsl_test::WindowIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        make_surface();
        if (!HasFatalFailure() && !physical.data_->device.getSurfaceSupportKHR(queue_family, surface->data_->surface))
            GTEST_SKIP() << "Selected queue cannot present";
    }
    vsl::SwapchainAccessor PresentationRetentionWindowIntegrationTest::make_chain() {
        const vsl::SwapchainCreateInfo info{.width = 64, .height = 48, .present_fences = present_fence_policy()};
        return l::SwapchainArena(device, *surface, info).view();
    }
    TEST(PresentationLayoutTest, StrictPolicyAddsOneAlignedFenceSlotForTheRuntimeImageCount) {
        constexpr vsl::storage::SwapchainLayout legacy(3);
        constexpr vsl::storage::SwapchainLayout strict(3, vsl::PresentFencePolicy::REQUIRE_FENCE);
        static_assert(!legacy.has_present_fences() && strict.has_present_fences());
        static_assert(strict.present_fence_offset() % vsl::storage::FENCE_SLOT_ALIGNMENT == 0);
        static_assert(strict.present_fence_offset() >= strict.image_offset(2) + vsl::storage::ResourceSlotLayout<vsl::impl::ImageImpl>::SIZE);
        static_assert(strict.bytes() >= strict.present_fence_offset() + vsl::storage::FENCE_SLOT_SIZE);
        EXPECT_THROW(legacy.present_fence_offset(), std::logic_error);
        EXPECT_THROW((vsl::storage::SwapchainLayout{3, static_cast<vsl::PresentFencePolicy>(99)}), std::invalid_argument);
    }
    TEST(PresentationStateTest, PendingAndSavedTerminalStatesNeverWaitForAnUnsubmittedOrReusedFence) {
        NativeProbe probe;
        s::PresentState state;
        state.strict = true;
        EXPECT_EQ(s::commit_present(state, vk::Result::eErrorOutOfHostMemory), s::PresentEnqueue::NOT_ENQUEUED);
        EXPECT_EQ(state.status, s::PresentStatus::PENDING);
        EXPECT_FALSE(s::poll_present(state));
        EXPECT_FALSE(s::wait_present(state));
        for (const auto status : {s::PresentStatus::COMPLETE, s::PresentStatus::FAILED, s::PresentStatus::CANCELLED, s::PresentStatus::LEGACY_RELEASED}) {
            state.status = status;
            EXPECT_TRUE(s::present_terminal(state));
            EXPECT_EQ(s::poll_present(state), status == s::PresentStatus::COMPLETE);
            EXPECT_EQ(s::wait_present(state), status == s::PresentStatus::COMPLETE);
        }
        EXPECT_EQ(probe.target_polls, 0u);
        EXPECT_EQ(probe.target_waits, 0u);
        s::FenceProof failed;
        failed.status = s::FenceProofStatus::NOT_ACQUIRED;
        EXPECT_TRUE(s::fence_proof_terminal(failed));
        EXPECT_FALSE(s::wait_fence_proof(failed));
    }
    TEST(PresentationStateTest, KnownDeviceLossPrecedesEvenAnAlreadySignaledLegacyReacquireProof) {
        auto registry = std::make_shared<l::SurfacePresentationDomain>();
        auto record = std::make_shared<l::PresentRecord>();
        record->state.status = s::PresentStatus::SUBMITTED;
        record->generation = std::make_shared<vsl::impl::SwapchainImpl>();
        record->image = 0;
        record->serial = 1;
        auto evidence = std::make_shared<l::AcquireEvidence>();
        evidence->generation = record->generation;
        evidence->generation_address = record->generation.get();
        evidence->proof.status = s::FenceProofStatus::COMPLETE;
        evidence->finished = true;
        evidence->image = 0;
        evidence->present_serial = 1;
        registry->records.push_back(record);
        registry->acquisitions.push_back(evidence);
        auto snapshot = l::snapshot_presentations(registry, {});
        l::finish_presentations(snapshot, vk::Result::eErrorDeviceLost);
        EXPECT_EQ(record->state.status, s::PresentStatus::FAILED);
        EXPECT_EQ(record->state.result, vk::Result::eErrorDeviceLost);
        EXPECT_TRUE(record->released);
        EXPECT_TRUE(registry->records.empty());
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, KnownDeviceLossIsSavedForAllSnapshotsBeforeAnyReleaseCallback) {
        auto manager = make_manager().independent(3);
        NativeProbe probe;
        auto chain = make_chain();
        auto registry = l::presentation_domain(chain.data_);
        vsl::Completion first, second;
        {
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            probe.target = chain.data_->presentation_context->fences->fences[phase.get_image_index()];
            phase.cancel();
            first = phase.completion();
        }
        ASSERT_EQ(registry->records.size(), 1u);
        const auto record = registry->records[0];
        const auto first_work = first.data_->work.lock();
        ASSERT_TRUE(first_work);
        manager.get_buffer().next();
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            second = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        manager.get_buffer().next();
        auto pending = manager.start_phase<vsl::ComputePhase>();
        unsigned releases = 0;
        const vsl_test::DeferredAction callback([&] {
            ++releases;
            EXPECT_EQ(first.status(), vsl::Completion::Status::FAILED);
            EXPECT_EQ(second.status(), vsl::Completion::Status::FAILED);
            EXPECT_EQ(first.failure(), vsl::Completion::Failure::DEVICE_LOST);
            EXPECT_EQ(second.failure(), vsl::Completion::Failure::DEVICE_LOST);
            EXPECT_EQ(record->state.status, s::PresentStatus::FAILED);
            EXPECT_EQ(pending.completion().status(), vsl::Completion::Status::PENDING);
        });
        first_work->completion_resources.push_back(callback.owner());
        const VkDevice native_device = device.data_->device;
        ASSERT_EQ(vkWaitForFences(native_device, 1, &probe.target, VK_TRUE, UINT64_MAX), VK_SUCCESS);
        const o::ScopedSubmissionNativeCalls idle({nullptr, +[](void*, VkDevice device) noexcept {
                                                       const auto result = vkDeviceWaitIdle(device);
                                                       return result == VK_SUCCESS ? VK_ERROR_DEVICE_LOST : result;
                                                   }});
        const auto polls = probe.target_polls, waits = probe.target_waits;
        EXPECT_THROW(manager.wait_idle(), vk::DeviceLostError);
        EXPECT_EQ(releases, 1u);
        EXPECT_EQ(probe.target_polls, polls);
        EXPECT_EQ(probe.target_waits, waits);
        EXPECT_TRUE(record->released);
        EXPECT_EQ(first.error_code(), VK_ERROR_DEVICE_LOST);
        EXPECT_EQ(second.error_code(), VK_ERROR_DEVICE_LOST);
        EXPECT_EQ(pending.completion().status(), vsl::Completion::Status::PENDING);
        pending.cancel();
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, GraphicsCompletionAndDeviceIdleDoNotReleaseAnUnobservedPresentFence) {
        auto manager = make_manager().independent(1);
        NativeProbe probe;
        std::weak_ptr<vsl::impl::SwapchainImpl> owner;
        std::shared_ptr<l::SurfacePresentationDomain> registry;
        vsl::Completion graphics;
        {
            auto chain = make_chain();
            const vsl::storage::SwapchainLayout layout(chain.images.size(), vsl::PresentFencePolicy::REQUIRE_FENCE);
            auto* pool = chain.data_->presentation_context->fences;
            ASSERT_NE(pool, nullptr);
            EXPECT_EQ(pool->fences.size(), chain.images.size());
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(pool) - reinterpret_cast<std::uintptr_t>(chain.data_.get()), layout.present_fence_offset());
            owner = chain.data_;
            registry = l::presentation_domain(chain.data_);
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            probe.target = pool->fences[phase.get_image_index()];
            phase.cancel();
            graphics = phase.completion();
        }
        ASSERT_TRUE(graphics.wait());
        ASSERT_EQ(registry->records.size(), 1u);
        EXPECT_FALSE(owner.expired());
        EXPECT_THROW(manager.wait_idle(), std::runtime_error);
        EXPECT_FALSE(owner.expired());
        EXPECT_EQ(registry->records[0]->state.status, s::PresentStatus::SUBMITTED);
        probe.hold = false;
        EXPECT_NO_THROW(manager.wait_idle());
        EXPECT_TRUE(registry->records.empty());
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, OutOfMemoryKeepsThePendingReservationWithoutWaitingAndCanRetryTheSamePresent) {
        auto manager = make_manager().independent(1);
        NativeProbe probe;
        auto chain = make_chain();
        auto registry = l::presentation_domain(chain.data_);
        vsl::SynchroManager sync(device);
        auto finished = sync.create_semaphore("present", chain.images.size());
        auto phase = manager.start_phase<vsl::DefaultPhase>(chain, std::nullopt, finished);
        const auto index = phase.get_image_index();
        probe.target = chain.data_->presentation_context->fences->fences[index];
        probe.forced = VK_ERROR_OUT_OF_HOST_MEMORY;
        phase.cancel();
        ASSERT_EQ(registry->records.size(), 1u);
        auto record = registry->records[0];
        EXPECT_EQ(record->state.status, s::PresentStatus::PENDING);
        EXPECT_EQ(chain.data_->presentation_context->images[index].present_serial, 0u);
        EXPECT_NO_THROW(manager.wait_idle());
        EXPECT_EQ(probe.target_waits, 0u);
        EXPECT_THROW(finished.destroy(), std::logic_error);
        probe.forced = VK_SUCCESS;
        EXPECT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_EQ(registry->records[0], record);
        EXPECT_EQ(record->state.status, s::PresentStatus::SUBMITTED);
        EXPECT_EQ(probe.calls, 2u);
        EXPECT_EQ(probe.fences[0], probe.fences[1]);
        probe.hold = false;
        manager.wait_idle();
        EXPECT_TRUE(record->released);
        EXPECT_NO_THROW(finished.destroy());
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, RejectedPresentationRequestsStillRetainTheirEnqueuedFence) {
        for (const auto error : {VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_SURFACE_LOST_KHR}) {
            auto manager = make_manager().independent(1);
            NativeProbe probe;
            auto chain = make_chain();
            auto registry = l::presentation_domain(chain.data_);
            vsl::RenderPass pass(chain);
            vsl::FrameBuffer target(chain, pass);
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            probe.target = chain.data_->presentation_context->fences->fences[phase.get_image_index()];
            probe.forced = error;
            phase << vsl::command::RenderPassBegin(pass, target) << vsl::command::RenderPassEnd{};
            EXPECT_EQ(phase.submit(), error == VK_ERROR_OUT_OF_DATE_KHR ? vsl::ComputePhase::SubmitResult::WANT_RECREATE_SWAPCHAIN
                                                                        : vsl::ComputePhase::SubmitResult::FAILED);
            ASSERT_EQ(registry->records.size(), 1u);
            const auto record = registry->records[0];
            EXPECT_EQ(record->state.status, s::PresentStatus::SUBMITTED);
            EXPECT_EQ(record->state.result, static_cast<vk::Result>(error));
            EXPECT_THROW(manager.wait_idle(), std::runtime_error);
            probe.hold = false;
            EXPECT_NO_THROW(manager.wait_idle());
            EXPECT_TRUE(record->released);
        }
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, RepeatedPresentationsReuseOnlyTheGenerationFencePoolAndBoundTheRegistry) {
        auto manager = make_manager().independent(1);
        NativeProbe probe;
        auto chain = make_chain();
        auto registry = l::presentation_domain(chain.data_);
        const auto fences = chain.data_->presentation_context->fences->fences;
        for (unsigned frame = 0; frame < 36; ++frame) {
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            const auto expected = fences[phase.get_image_index()];
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
            EXPECT_EQ(probe.fences[frame], static_cast<VkFence>(expected));
            EXPECT_LE(registry->records.size(), chain.images.size() + 1);
            EXPECT_LE(registry->acquisitions.size(), 2u);
        }
        EXPECT_EQ(probe.calls, 36u);
        manager.wait_idle();
        EXPECT_TRUE(registry->records.empty());
    }
    TEST_F(LegacyPresentationRetentionWindowIntegrationTest, AnotherCommandDomainReacquiresTheNewGenerationAndReclaimsTheRetiredOne) {
        auto first = make_manager().independent(1);
        auto second = make_manager().independent(1);
        auto old = make_chain();
        const auto registry = l::presentation_domain(old.data_);
        const std::weak_ptr<vsl::impl::SwapchainImpl> owner = old.data_;
        {
            auto phase = first.start_phase<vsl::DefaultPhase>(old);
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
        }
        const vsl::SwapchainCreateInfo info{.width = 64, .height = 48};
        auto next = l::SwapchainArena(device, *surface, info, old).view();
        old = {};
        ASSERT_FALSE(owner.expired());
        EXPECT_EQ(registry, l::presentation_domain(next.data_));
        for (size_t frame = 0; frame < next.images.size() + 2 && !owner.expired(); ++frame) {
            auto phase = second.start_phase<vsl::DefaultPhase>(next);
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(owner.expired());
        first.wait_idle();
        second.wait_idle();
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, CompletedGenerationReleaseCanDestroyTheCurrentCancellingPhase) {
        auto first = make_manager().independent(1);
        auto second = make_manager().independent(1);
        NativeProbe probe;
        std::unique_ptr<vsl::DefaultPhase> phase;
        vsl::Completion returned;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            ASSERT_TRUE(phase);
            returned = phase->completion();
            phase.reset();
        });
        vsl::SwapchainAccessor next;
        {
            const vsl::SwapchainCreateInfo info{.width = 64, .height = 48, .present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE};
            l::SwapchainArena arena(device, *surface, info, {}, [&](const auto& layout) {
                return callback_memory(layout, callback.owner());
            });
            auto old = arena.view();
            {
                auto previous = first.start_phase<vsl::DefaultPhase>(old);
                probe.target = old.data_->presentation_context->fences->fences[previous.get_image_index()];
                previous.cancel();
                ASSERT_TRUE(previous.completion().wait());
            }
            ASSERT_EQ(device.data_->device.waitForFences(vk::Fence(probe.target), true, UINT64_MAX), vk::Result::eSuccess);
            next = l::SwapchainArena(device, *surface, info, old).view();
        }
        ASSERT_FALSE(invoked);
        phase = std::make_unique<vsl::DefaultPhase>(second, next);
        probe.hold = false;
        EXPECT_NO_THROW(phase->cancel());
        EXPECT_TRUE(invoked);
        EXPECT_FALSE(phase);
        EXPECT_TRUE(returned.wait());
        first.wait_idle();
        second.wait_idle();
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, SavedTerminalRecordsAreReclaimedByTheNextWaitIdleWithoutRewaitingTheFence) {
        auto manager = make_manager().independent(1);
        NativeProbe probe;
        std::weak_ptr<vsl::impl::SwapchainImpl> owner;
        std::shared_ptr<l::PresentRecord> record;
        {
            auto chain = make_chain();
            owner = chain.data_;
            auto registry = l::presentation_domain(chain.data_);
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            probe.target = chain.data_->presentation_context->fences->fences[phase.get_image_index()];
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
            ASSERT_EQ(registry->records.size(), 1u);
            record = registry->records[0];
        }
        ASSERT_EQ(device.data_->device.waitForFences(vk::Fence(probe.target), true, UINT64_MAX), vk::Result::eSuccess);
        probe.hold = false;
        ASSERT_TRUE(s::poll_present(record->state));
        ASSERT_FALSE(record->released);
        EXPECT_FALSE(owner.expired());
        const auto waits = probe.target_waits;
        manager.wait_idle();
        EXPECT_EQ(probe.target_waits, waits);
        EXPECT_TRUE(record->released);
        EXPECT_TRUE(owner.expired());
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, PresentPoolAliasesProtectPendingUseAndSaveCompletionBeforeNativeReset) {
        auto manager = make_manager().independent(1);
        auto chain = make_chain();
        auto registry = l::presentation_domain(chain.data_);
        vsl::FenceHolder fences(std::shared_ptr<vsl::impl::FenceImpl>(chain.data_, chain.data_->presentation_context->fences));
        NativeProbe probe;
        auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
        const auto index = phase.get_image_index();
        probe.target = fences.data_->fences[index];
        probe.forced = VK_ERROR_OUT_OF_HOST_MEMORY;
        phase.cancel();
        ASSERT_TRUE(phase.completion().wait());
        ASSERT_EQ(registry->records.size(), 1u);
        const auto record = registry->records[0];
        bool pending_rejected = false;
        try {
            (void)fences.wait(index, 0);
        } catch (const std::logic_error&) {
            pending_rejected = true;
        }
        EXPECT_TRUE(pending_rejected);
        if (!pending_rejected) {
            // 旧実装との比較でもnative poolを破棄せず、実presentと待機を済ませる。
            probe.forced = VK_SUCCESS;
            ASSERT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_EQ(device.data_->device.waitForFences(vk::Fence(probe.target), true, UINT64_MAX), vk::Result::eSuccess);
            s::save_present_observation(record->state, vk::Result::eSuccess);
            manager.wait_idle();
            return;
        }
        EXPECT_THROW(fences.reset(index), std::logic_error);
        EXPECT_THROW(fences.destroy(), std::logic_error);
        probe.forced = VK_SUCCESS;
        ASSERT_EQ(phase.present(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_THROW(fences.reset(index), std::logic_error);
        EXPECT_THROW(fences.destroy(), std::logic_error);
        ASSERT_EQ(device.data_->device.waitForFences(vk::Fence(probe.target), true, UINT64_MAX), vk::Result::eSuccess);
        probe.hold = false;
        EXPECT_NO_THROW(fences.reset(index));
        EXPECT_EQ(record->state.status, s::PresentStatus::COMPLETE);
        EXPECT_FALSE(fences.wait(index, 0));
        EXPECT_TRUE(s::poll_present(record->state));
        // 比較実装が旧Recordを保存しなかった場合も、実待機済みの証拠を保存して後処理する。
        s::save_present_observation(record->state, vk::Result::eSuccess);
        manager.wait_idle();
        EXPECT_TRUE(record->released);
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, ShutdownCallbacksCannotReattachUntilTheSameSurfaceDomainFinishesRetiring) {
        vsl::SwapchainAccessor next;
        bool rejected = false;
        const vsl_test::DeferredAction callback([&] {
            EXPECT_THROW(l::presentation_domain(next.data_), std::logic_error);
            rejected = true;
        });
        std::weak_ptr<l::SurfacePresentationDomain> old_domain;
        {
            auto manager = make_manager().independent(1);
            NativeProbe probe;
            const vsl::SwapchainCreateInfo info{.width = 64, .height = 48, .present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE};
            {
                l::SwapchainArena arena(device, *surface, info, {}, [&](const auto& layout) {
                    return callback_memory(layout, callback.owner());
                });
                auto chain = arena.view();
                old_domain = l::presentation_domain(chain.data_);
                {
                    auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
                    probe.target = chain.data_->presentation_context->fences->fences[phase.get_image_index()];
                    phase.cancel();
                    ASSERT_TRUE(phase.completion().wait());
                }
                next = l::SwapchainArena(device, *surface, info, chain).view();
            }
            ASSERT_FALSE(rejected);
            probe.hold = false;
        }
        EXPECT_TRUE(rejected);
        EXPECT_TRUE(old_domain.expired());
        EXPECT_TRUE(l::presentation_domain(next.data_));
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, WaitIdleKeepsNewPendingAndSubmittedWorkRegisteredByRetirementCallbacks) {
        for (const bool submit : {false, true}) {
            auto manager = make_manager().independent(1);
            NativeProbe probe;
            vsl::SwapchainAccessor next;
            std::unique_ptr<vsl::DefaultPhase> phase;
            bool invoked = false;
            const vsl_test::DeferredAction callback([&] {
                invoked = true;
                phase = std::make_unique<vsl::DefaultPhase>(manager, next);
                probe.target = next.data_->presentation_context->fences->fences[phase->get_image_index()];
                probe.hold = true;
                if (submit)
                    phase->cancel();
            });
            const vsl::SwapchainCreateInfo info{.width = 64, .height = 48, .present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE};
            {
                l::SwapchainArena arena(device, *surface, info, {}, [&](const auto& layout) {
                    return callback_memory(layout, callback.owner());
                });
                auto chain = arena.view();
                {
                    auto previous = manager.start_phase<vsl::DefaultPhase>(chain);
                    probe.target = chain.data_->presentation_context->fences->fences[previous.get_image_index()];
                    previous.cancel();
                    ASSERT_TRUE(previous.completion().wait());
                }
                next = l::SwapchainArena(device, *surface, info, chain).view();
            }
            probe.hold = false;
            EXPECT_NO_THROW(manager.wait_idle());
            ASSERT_TRUE(invoked);
            ASSERT_TRUE(phase);
            EXPECT_EQ(phase->completion().data_->status, submit ? vsl::Completion::Status::SUBMITTED : vsl::Completion::Status::PENDING);
            const auto registry = l::presentation_domain(next.data_);
            if (submit) {
                ASSERT_EQ(registry->records.size(), 1u);
                EXPECT_EQ(registry->records[0]->state.status, s::PresentStatus::SUBMITTED);
            } else
                EXPECT_TRUE(registry->records.empty());
            if (!submit)
                phase->cancel();
            phase.reset();
            probe.hold = false;
            manager.wait_idle();
        }
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, PresentAndIdleOnlyCollectQuarantineForTheirCanonicalSurface) {
        ClosingWindow peer("Other presentation scope", 64, 48);
        auto peer_surface = peer.add_plugin<vsl::Surface>(vsl::VulkanAccessor{instance});
        auto manager = make_manager().independent(1);
        NativeProbe probe;
        const auto before = l::quarantined_presentation_count();
        std::weak_ptr<vsl::impl::SwapchainImpl> peer_owner;
        {
            auto other = make_manager().independent(1);
            const vsl::SwapchainCreateInfo info{.width = 64, .height = 48, .present_fences = vsl::PresentFencePolicy::REQUIRE_FENCE};
            auto chain = l::SwapchainArena(device, *peer_surface, info).view();
            peer_owner = chain.data_;
            auto phase = other.start_phase<vsl::DefaultPhase>(chain);
            probe.target = chain.data_->presentation_context->fences->fences[phase.get_image_index()];
            probe.forced = VK_ERROR_UNKNOWN;
            probe.unknown_before = true;
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
        }
        ASSERT_EQ(l::quarantined_presentation_count(), before + 1);
        const auto polls = probe.target_polls;
        probe.forced = VK_SUCCESS;
        probe.device_lost = true;
        {
            auto chain = make_chain();
            auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
            phase.cancel();
            ASSERT_TRUE(phase.completion().wait());
        }
        manager.wait_idle();
        EXPECT_EQ(probe.target_polls, polls);
        EXPECT_FALSE(peer_owner.expired());
        EXPECT_EQ(l::quarantined_presentation_count(), before + 1);
        l::collect_quarantined_presentations(peer_surface->data_, device.data_->device);
        EXPECT_EQ(l::quarantined_presentation_count(), before);
        EXPECT_TRUE(peer_owner.expired());
    }
    TEST_F(PresentationRetentionWindowIntegrationTest, UnknownEnqueueIsQuarantinedWithoutWaitingForAnUnsubmittedFence) {
        const auto before = l::quarantined_presentation_count();
        for (const bool before_enqueue : {false, true}) {
            NativeProbe probe;
            probe.unknown_before = before_enqueue;
            std::weak_ptr<vsl::impl::SwapchainImpl> owner;
            {
                auto manager = make_manager().independent(1);
                auto chain = make_chain();
                owner = chain.data_;
                auto phase = manager.start_phase<vsl::DefaultPhase>(chain);
                probe.target = chain.data_->presentation_context->fences->fences[phase.get_image_index()];
                probe.forced = VK_ERROR_UNKNOWN;
                phase.cancel();
                ASSERT_TRUE(phase.completion().wait());
                EXPECT_THROW(manager.wait_idle(), std::runtime_error);
            }
            EXPECT_EQ(probe.target_waits, 0u);
            EXPECT_EQ(l::quarantined_presentation_count(), before + 1);
            EXPECT_FALSE(owner.expired());
            if (before_enqueue)
                probe.device_lost = true; // 未送信Fenceをsignalせず、別の終端情報で回収する。
            else
                ASSERT_EQ(device.data_->device.waitForFences(vk::Fence(probe.target), true, UINT64_MAX), vk::Result::eSuccess);
            probe.hold = false;
            l::collect_quarantined_presentations(surface->data_, device.data_->device);
            EXPECT_EQ(l::quarantined_presentation_count(), before);
            EXPECT_TRUE(owner.expired());
        }
    }
} // namespace
