#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include "support/deferred_action.hpp"
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/sync/submission_completion.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    namespace s = vsl::storage;
    namespace l = vsl::lifetime;
    constexpr std::array<t::ResourceRequest, 3> REQUESTS{{{{1}, s::ResourceKind::COMMAND_BUFFER}, {{2}, s::ResourceKind::COMMAND_BUFFER}, {{3}, s::ResourceKind::COMMAND_POOL}}};
    constexpr t::ResourcePlan<3> PLAN(REQUESTS);
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    constexpr vsl::BufferCreateInfo BUFFER_INFO{.bytes = 16, .usage = USAGE, .properties = vsl::MemoryProperty::HOST_VISIBLE};
    class CommandArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        std::array<l::ResourceInit, 3> inputs() const;
    };
    // queueへ送信せず、未完了判定をunsignaled native fenceで決定的に検査する。
    struct UnsignaledWork {
        std::shared_ptr<vsl::impl::SubmissionWork> work;
        explicit UnsignaledWork(vsl::LogicalDeviceAccessor device);
        ~UnsignaledWork();
    };
    std::array<l::ResourceInit, 3> CommandArenaVulkanIntegrationTest::inputs() const {
        return {l::CommandBufferInit{t::ResourceId{3}, 2}, l::CommandBufferInit{t::ResourceId{3}, 3}, vsl::CommandPoolCreateInfo{true}};
    }
    UnsignaledWork::UnsignaledWork(vsl::LogicalDeviceAccessor device) : work(std::make_shared<vsl::impl::SubmissionWork>()) {
        work->device = device.data_;
        work->owned_fence = std::make_shared<vk::UniqueFence>(device.data_->device.createFenceUnique({}));
        work->fence = **work->owned_fence;
        work->result->status = vsl::Completion::Status::SUBMITTED;
        work->result->work = work;
    }
    UnsignaledWork::~UnsignaledWork() {
        vsl::sync::publish_terminal(*work, vsl::Completion::Status::COMPLETE);
    }

    TEST(CommandPlacementTest, GeneratedSlotsIncludeContextsAndPreserveAlignment) {
        static_assert(sizeof(vsl::impl::CommandPoolImpl) == s::COMMAND_POOL_IMPL_SIZE);
        static_assert(sizeof(vsl::impl::CommandBufferImpl) == s::COMMAND_BUFFER_IMPL_SIZE);
        static_assert(sizeof(l::CommandPoolSlotStorage) == s::COMMAND_POOL_SLOT_SIZE);
        static_assert(sizeof(l::CommandBufferSlotStorage) == s::COMMAND_BUFFER_SLOT_SIZE);
        static_assert(!std::is_copy_constructible_v<vsl::impl::CommandPoolImpl>);
        static_assert(!std::is_copy_constructible_v<vsl::impl::CommandBufferImpl>);
        static_assert(s::ResourceSlotLayout<vsl::impl::CommandBufferImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        EXPECT_EQ(PLAN.bytes(), 2 * s::COMMAND_BUFFER_SLOT_SIZE + s::COMMAND_POOL_SLOT_SIZE);
        EXPECT_GT(s::COMMAND_BUFFER_SLOT_SIZE, s::COMMAND_BUFFER_IMPL_SIZE);
        EXPECT_EQ(PLAN.placement({2}).offset, s::COMMAND_BUFFER_SLOT_SIZE);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes() - 1, PLAN.alignment()), std::invalid_argument);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes(), PLAN.alignment() / 2), std::invalid_argument);
    }
    TEST(CommandPlacementTest, EmptyFacadesRejectOwningGettersAndManagerAttachment) {
        vsl::CommandPool pool(CommandAccess::Pool(nullptr));
        vsl::CommandBuffer buffer(CommandAccess::RawBuffer(nullptr));
        EXPECT_THROW(pool.get_device(), std::invalid_argument);
        EXPECT_THROW(pool.owner_ref(), std::invalid_argument);
        EXPECT_THROW(buffer.get_pool(), std::invalid_argument);
        EXPECT_THROW(buffer.owner_ref(), std::invalid_argument);
        EXPECT_THROW(vsl::CommandManager::FromBuffer(buffer), std::logic_error);
    }
    TEST_F(CommandArenaVulkanIntegrationTest, TwoRingsShareStorageAndKeepIndependentCanonicalDomains) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto first = arena.command_buffer({1}), second = arena.command_buffer({2});
        const auto base = reinterpret_cast<std::uintptr_t>(CommandAccess::Data(first).get());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(CommandAccess::Data(second).get()) - base, PLAN.placement({2}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(CommandAccess::Data(arena.command_pool({3})).get()) - base, PLAN.placement({3}).offset);
        const auto context = reinterpret_cast<std::uintptr_t>(CommandAccess::Data(first)->owner_context);
        EXPECT_GE(context, base + s::COMMAND_BUFFER_IMPL_SIZE);
        EXPECT_LT(context, base + s::COMMAND_BUFFER_SLOT_SIZE);
        EXPECT_EQ(first.get_size(), 2u);
        EXPECT_EQ(second.get_size(), 3u);
        EXPECT_FALSE(CommandAccess::Data(first).owner_before(CommandAccess::Data(second)));
        EXPECT_FALSE(CommandAccess::Data(second).owner_before(CommandAccess::Data(first)));
        const auto first_domain = l::CommandAccess::Owner(first), second_domain = l::CommandAccess::Owner(second);
        EXPECT_NE(first_domain, second_domain);
        EXPECT_EQ(l::CommandAccess::Owner(arena.command_buffer({1})), first_domain);
        EXPECT_EQ(CommandAccess::Data(first.get_pool()), CommandAccess::Data(arena.command_pool({3})));
        EXPECT_TRUE(first.owner_ref().managed());
        EXPECT_TRUE(arena.command_pool({3}).owner_ref().managed());
        auto manager = vsl::CommandManager::FromBuffer(first);
        auto peer = vsl::CommandManager::FromBuffer(second);
        auto phase = manager.start_phase<vsl::ComputePhase>();
        auto other = peer.start_phase<vsl::ComputePhase>();
        EXPECT_NE(phase.completion().data_, other.completion().data_);
        EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_EQ(other.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_TRUE(phase.completion().wait());
        EXPECT_TRUE(other.completion().wait());
        EXPECT_THROW(arena.command_pool({1}), std::invalid_argument);
        EXPECT_THROW(arena.command_buffer({9}), std::out_of_range);
    }
    TEST_F(CommandArenaVulkanIntegrationTest, TypedCopySurvivesWrappersAndDoesNotCycleThroughItsOwnArena) {
        constexpr std::array<t::ResourceRequest, 4> requests{{{{1}, s::ResourceKind::COMMAND_BUFFER}, {{2}, s::ResourceKind::COMMAND_POOL}, {{3}, s::ResourceKind::BUFFER}, {{4}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<4> plan(requests);
        const std::array<l::ResourceInit, 4> initializers{l::CommandBufferInit{t::ResourceId{2}, 1}, vsl::CommandPoolCreateInfo{}, BUFFER_INFO, BUFFER_INFO};
        std::optional<vsl::CommandManager> manager;
        std::optional<vsl::command::CopyBuffer> command;
        std::weak_ptr<void> root;
        vsl::BufferRef destination;
        const std::array<std::uint32_t, 4> expected{2, 4, 8, 16};
        {
            l::ResourceArena arena(device, plan, initializers);
            root = arena.owner();
            manager = vsl::CommandManager::FromBuffer(arena.command_buffer({1}));
            View source(arena.buffer({3})), target(arena.buffer({4}));
            source.copy(expected);
            destination = target.backend().ref();
            command.emplace(source.copy_to(target));
        }
        vsl::Completion result;
        {
            auto phase = manager->start_phase<vsl::ComputePhase>();
            phase << *command;
            command.reset();
            const auto native = manager->get_buffer();
            native.retain(CommandAccess::Data(native));
            ASSERT_EQ(CommandAccess::Data(native)->recording_context->work->completion_resources.size(), 1u);
            result = phase.completion();
            manager.reset();
            EXPECT_FALSE(root.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(result.wait());
            std::array<std::uint32_t, 4> actual{};
            destination.read_into(std::as_writable_bytes(std::span(actual)));
            EXPECT_EQ(actual, expected);
        }
        EXPECT_TRUE(root.expired());
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
    }
    TEST_F(CommandArenaVulkanIntegrationTest, CancelledResultDoesNotOwnTheArenaThatWasRetainedByItsWork) {
        std::weak_ptr<void> root;
        vsl::Completion cancelled;
        {
            l::ResourceArena arena(device, PLAN, inputs());
            root = arena.owner();
            auto manager = vsl::CommandManager::FromBuffer(arena.command_buffer({1}));
            auto phase = manager.start_phase<vsl::ComputePhase>();
            manager.get_buffer().retain(arena.owner());
            cancelled = phase.completion();
            phase.cancel();
        }
        EXPECT_TRUE(root.expired());
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(cancelled.data_->work.expired());
    }
    TEST_F(CommandArenaVulkanIntegrationTest, LaterRingFailureRollsBackThePoolAndAllowsRegionReuse) {
        s::ArenaMemory memory(PLAN.bytes(), PLAN.alignment());
        auto invalid = inputs();
        invalid[1] = l::CommandBufferInit{t::ResourceId{3}, 0};
        EXPECT_THROW((l::ResourceArena(device, PLAN, invalid, memory)), std::invalid_argument);
        l::ResourceArena arena(device, PLAN, inputs(), memory);
        EXPECT_EQ(arena.command_buffer({1}).get_size(), 2u);
        EXPECT_EQ(arena.command_buffer({2}).get_size(), 3u);
        auto manager = vsl::CommandManager::FromBuffer(arena.command_buffer({1}));
        auto phase = manager.start_phase<vsl::ComputePhase>();
        EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_TRUE(phase.completion().wait());
    }
    TEST_F(CommandArenaVulkanIntegrationTest, FailedAttachmentAndRetirementDoNotPublishOrReplaceAnotherDomain) {
        l::ResourceArena arena(device, PLAN, inputs());
        const auto native = CommandAccess::Data(arena.command_buffer({1}));
        EXPECT_EQ(native->recording_context, nullptr);
        const auto fail = +[](std::shared_ptr<vsl::impl::CommandBufferImpl>) -> std::shared_ptr<l::CommandDomain> { throw std::bad_alloc(); };
        EXPECT_THROW(l::CommandAccess::Attach(native, fail), std::bad_alloc);
        EXPECT_EQ(native->recording_context, nullptr);
        auto peer = arena.command_buffer({2});
        const auto peer_domain = l::CommandAccess::Owner(peer);
        bool rejected = false;
        const vsl_test::DeferredAction callback([&] {
            EXPECT_TRUE(native->recording_context);
            EXPECT_THROW(arena.command_buffer({1}), std::logic_error);
            rejected = true;
            EXPECT_EQ(l::CommandAccess::Owner(peer), peer_domain);
        });
        {
            auto buffer = arena.command_buffer({1});
            auto manager = vsl::CommandManager::FromBuffer(buffer);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            buffer.retain(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(rejected);
        EXPECT_EQ(native->recording_context, nullptr);
        auto reattached = arena.command_buffer({1});
        EXPECT_NE(l::CommandAccess::Owner(reattached), peer_domain);
        EXPECT_EQ(l::CommandAccess::Owner(peer), peer_domain);
        EXPECT_NO_THROW(reattached.reset());
    }
    TEST_F(CommandArenaVulkanIntegrationTest, ImportsKeepOriginalOwnersAndOnlyFacadesKeepTheirDomains) {
        constexpr std::array<t::ResourceRequest, 2> requests{{{{10}, s::ResourceKind::COMMAND_BUFFER, true}, {{20}, s::ResourceKind::COMMAND_POOL, true}}};
        constexpr t::ResourcePlan<2> plan(requests);
        std::optional<l::ResourceArena> imported;
        std::weak_ptr<l::CommandDomain> previous;
        std::weak_ptr<void> original_root, importer_root;
        vsl::CommandPool saved(CommandAccess::Pool(nullptr));
        {
            l::ResourceArena source(device, PLAN, inputs());
            auto buffer = source.command_buffer({1});
            previous = l::CommandAccess::Owner(buffer);
            original_root = source.owner();
            const std::array<l::ResourceInit, 2> initializers{buffer, source.command_pool({3})};
            imported.emplace(device, plan, initializers);
            importer_root = imported->owner();
            auto from_import = imported->command_buffer({10});
            EXPECT_EQ(l::CommandAccess::Owner(from_import), previous.lock());
            EXPECT_FALSE(CommandAccess::Data(from_import).owner_before(CommandAccess::Data(buffer)));
            EXPECT_FALSE(CommandAccess::Data(buffer).owner_before(CommandAccess::Data(from_import)));
            saved = from_import.get_pool();
            EXPECT_EQ(CommandAccess::Data(saved), CommandAccess::Data(source.command_pool({3})));
        }
        EXPECT_TRUE(previous.expired());
        EXPECT_FALSE(original_root.expired());
        auto renewed = imported->command_buffer({10});
        EXPECT_TRUE(CommandAccess::Data(renewed)->recording_context);
        EXPECT_EQ(CommandAccess::Data(renewed.get_pool()), CommandAccess::Data(saved));
        imported.reset();
        EXPECT_TRUE(importer_root.expired());
        EXPECT_FALSE(original_root.expired());
    }
    TEST_F(CommandArenaVulkanIntegrationTest, HeapPoolGettersKeepTheOriginalParentAndPublishFixedEdges) {
        std::optional<vsl::CommandPool> saved;
        vsl::LogicalDeviceAccessor device_only;
        std::weak_ptr<void> pool_root, buffer_root;
        {
            vsl::CommandPool pool(device);
            vsl::CommandBuffer buffer(pool, 2);
            pool_root = CommandAccess::Data(pool);
            buffer_root = CommandAccess::Data(buffer);
            saved = buffer.get_pool();
            device_only = saved->get_device();
            EXPECT_EQ(device_only.data_, device.data_);
            EXPECT_FALSE(CommandAccess::Data(*saved).owner_before(CommandAccess::Data(pool)));
            EXPECT_FALSE(CommandAccess::Data(pool).owner_before(CommandAccess::Data(*saved)));
            const std::array<l::OwnerRef, 1> reverse{buffer.owner_ref()};
            EXPECT_THROW(CommandAccess::Data(pool)->owner_node->check_additions(reverse), std::invalid_argument);
        }
        EXPECT_TRUE(buffer_root.expired());
        EXPECT_FALSE(pool_root.expired());
        saved.reset();
        EXPECT_TRUE(pool_root.expired());
        EXPECT_EQ(device_only.data_, device.data_);
    }
    TEST_F(CommandArenaVulkanIntegrationTest, ForeignControlBlocksCannotImpersonateTheSameRingOrPool) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto buffer = arena.command_buffer({1});
        const auto foreign = std::shared_ptr<vsl::impl::CommandBufferImpl>(std::make_shared<int>(0), CommandAccess::Data(buffer).get());
        EXPECT_THROW(vsl::CommandBuffer{CommandAccess::RawBuffer(foreign)}, std::logic_error);
        EXPECT_THROW(l::CommandAccess::Attach(foreign), std::invalid_argument);
        auto forged = buffer;
        CommandAccess::Data(forged) = foreign;
        EXPECT_THROW(vsl::CommandManager::FromBuffer(forged), std::logic_error);
        auto manager = vsl::CommandManager::FromBuffer(buffer);
        CommandAccess::Data(manager)->command_buffer = foreign;
        EXPECT_THROW(manager.get_buffer(), std::logic_error);
        CommandAccess::Data(manager)->command_buffer = CommandAccess::Data(buffer);
        const auto native_pool = CommandAccess::Data(arena.command_pool({3}));
        vsl::CommandPool forged_pool(CommandAccess::Pool(std::shared_ptr<vsl::impl::CommandPoolImpl>(std::make_shared<int>(0), native_pool.get())));
        EXPECT_THROW(forged_pool.get_device(), std::invalid_argument);
        EXPECT_THROW((vsl::CommandBuffer(forged_pool)), std::invalid_argument);
    }
    TEST_F(CommandArenaVulkanIntegrationTest, RecordingRejectsMutationAndResetKeepsSubmissionPinsUntilReplacement) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto manager = vsl::CommandManager::FromBuffer(arena.command_buffer({1}));
        auto buffer = manager.get_buffer();
        std::weak_ptr<int> retained;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto owner = std::make_shared<int>(9);
            retained = owner;
            buffer.retain(owner);
            EXPECT_THROW(buffer.reset(), std::logic_error);
            EXPECT_THROW(buffer.reset(0), std::logic_error);
            EXPECT_THROW(buffer.next(), std::logic_error);
            EXPECT_EQ(buffer.get_current_buffer_idx(), 0u);
            EXPECT_EQ(phase.completion().status(), vsl::Completion::Status::PENDING);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_NO_THROW(buffer.reset());
        EXPECT_FALSE(retained.expired());
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_TRUE(retained.expired());
    }
    TEST_F(CommandArenaVulkanIntegrationTest, ResetAllChecksEverySlotBeforeTouchingExecutableCommands) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto buffer = arena.command_buffer({1});
        const auto domain = l::CommandAccess::Owner(buffer);
        UnsignaledWork pending(device);
        domain->submissions.at(1) = pending.work;
        vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE> source(device, 4), target(device, 4);
        const std::uint32_t expected = 97;
        ASSERT_TRUE(source.copy(expected));
        auto native = CommandAccess::Data(buffer)->command_buffers.at(0);
        native.begin(vk::CommandBufferBeginInfo{});
        native.copyBuffer(source.data_->buffer, target.data_->buffer, vk::BufferCopy{.size = 4});
        const vk::MemoryBarrier visible{.srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eHostRead};
        native.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, visible, {}, {});
        native.end();
        EXPECT_THROW(buffer.reset(), std::logic_error);
        EXPECT_THROW(buffer.reset(1), std::logic_error);
        auto fence = device.data_->device.createFenceUnique({});
        const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &native};
        device.data_->device.getQueue(queue_family, 0).submit(submit, *fence);
        ASSERT_EQ(device.data_->device.waitForFences(*fence, true, UINT64_MAX), vk::Result::eSuccess);
        std::uint32_t actual = 0;
        target.read_into(std::as_writable_bytes(std::span(&actual, 1)));
        EXPECT_EQ(actual, expected);
        EXPECT_NO_THROW(buffer.next());
        EXPECT_EQ(buffer.get_current_buffer_idx(), 1u);
    }
    TEST_F(CommandArenaVulkanIntegrationTest, ResetCallbackCannotReenterAndCanDestroyItsOriginalFacades) {
        l::ResourceArena arena(device, PLAN, inputs());
        std::optional<vsl::CommandManager> manager(vsl::CommandManager::FromBuffer(arena.command_buffer({1})));
        std::optional<vsl::CommandBuffer> buffer(manager->get_buffer());
        std::weak_ptr<l::CommandDomain> domain = l::CommandAccess::Owner(*buffer);
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            EXPECT_THROW(manager->start_phase<vsl::ComputePhase>(), std::logic_error);
            EXPECT_THROW(buffer->reset(), std::logic_error);
            EXPECT_THROW(buffer->next(), std::logic_error);
            manager.reset();
            buffer.reset();
        });
        {
            auto phase = manager->start_phase<vsl::ComputePhase>();
            buffer->retain_until_complete(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        device.data_->device.waitIdle();
        buffer->reset(0);
        EXPECT_TRUE(invoked);
        EXPECT_FALSE(buffer);
        EXPECT_FALSE(manager);
        EXPECT_TRUE(domain.expired());
        EXPECT_NO_THROW(arena.command_buffer({1}));
    }
    TEST_F(CommandArenaVulkanIntegrationTest, PrepareDetectsACompleteRotationBackToTheSameSlot) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto manager = vsl::CommandManager::FromBuffer(arena.command_buffer({1}));
        const vsl_test::DeferredAction callback([&] {
            manager.get_buffer().next();
            manager.get_buffer().next();
        });
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            manager.get_buffer().retain_until_complete(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        device.data_->device.waitIdle();
        EXPECT_THROW(manager.start_phase<vsl::ComputePhase>(), std::logic_error);
        EXPECT_EQ(manager.get_current_buffer_idx(), 0u);
        auto next = manager.start_phase<vsl::ComputePhase>();
        EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        EXPECT_TRUE(next.completion().wait());
    }
} // namespace
