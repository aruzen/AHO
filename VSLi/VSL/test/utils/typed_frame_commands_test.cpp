#include "support/vulkan_fixture.hpp"
#include "support/deferred_action.hpp"
#include <VSL/utils/typed_frame_resources.hpp>
#include <VSL/lifetime/command_domain.hpp>

namespace {
    namespace t = vsl::typed;
    struct FrameValue {
        size_t slot;
        int updates = 0;
    };
    struct TrackedFrame {
        std::shared_ptr<int> value;
    };
    class TypedFrameCommandsVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest, public testing::WithParamInterface<bool> {
    protected:
        bool validate_vulkan() const override { return true; }
        t::TypedFrameCommands make_commands(std::uint32_t count);
    };
    t::TypedFrameCommands TypedFrameCommandsVulkanIntegrationTest::make_commands(std::uint32_t count) {
        if (GetParam())
            return t::TypedFrameCommands(device, count);
        return t::TypedFrameCommands(device, count, t::HeapStorage{});
    }
    TEST(TypedFrameCommandsTest, DefaultsToArenaAndFacadeIsMovableButNotCopyable) {
        static_assert(std::same_as<t::DefaultStorage, t::ArenaStorage<>>);
        static_assert(std::is_constructible_v<t::TypedFrameCommands, vsl::LogicalDeviceAccessor, std::uint32_t>);
        static_assert(std::is_constructible_v<t::TypedFrameCommands, vsl::LogicalDeviceAccessor, std::uint32_t, t::HeapStorage>);
        static_assert(std::is_constructible_v<t::TypedFrameCommands, vsl::LogicalDeviceAccessor, std::uint32_t, t::ArenaStorage<>>);
        static_assert(!std::is_copy_constructible_v<t::TypedFrameCommands> && !std::is_copy_assignable_v<t::TypedFrameCommands>);
        static_assert(std::is_nothrow_move_constructible_v<t::TypedFrameCommands> && std::is_nothrow_move_assignable_v<t::TypedFrameCommands>);
        static_assert(std::same_as<decltype(std::declval<const t::TypedFrameCommands&>().start_compute()), vsl::ComputePhase>);
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, ThreeSlotsReuseTheirPlacedFencesAndTheSameTypedFrameIdentities) {
        auto commands = make_commands(3);
        auto manager = commands.manager();
        auto buffer = manager.get_buffer();
        auto fence = commands.fence();
        EXPECT_EQ(commands.frame_count(), 3u);
        EXPECT_EQ(fence.size(), 3u);
        EXPECT_EQ(commands.manager().data_, manager.data_);
        const bool same_owner = !buffer.data_.owner_before(fence.data_) && !fence.data_.owner_before(buffer.data_);
        EXPECT_EQ(same_owner, GetParam());
        t::TypedFrameResources<FrameValue> frames(manager, [](size_t slot) { return FrameValue{slot}; });
        std::array<const FrameValue*, 3> values{};
        const auto handles = fence.data_->fences;
        for (size_t iteration = 0; iteration < 12; ++iteration) {
            auto phase = commands.start_compute();
            EXPECT_EQ(phase.frame_slot, iteration % 3);
            const auto* work = buffer.data_->recording_context->work;
            EXPECT_FALSE(work->owned_fence);
            EXPECT_EQ(work->external_fence, fence.data_.get());
            EXPECT_EQ(work->fence, handles[iteration % 3]);
            frames.update(phase, [&](FrameValue& value) {
                EXPECT_EQ(value.slot, iteration % 3);
                EXPECT_EQ(value.updates, int(iteration / 3));
                ++value.updates;
            });
            const auto& value = frames.get(phase);
            if (iteration < 3)
                values[iteration] = &value;
            EXPECT_EQ(&value, values[iteration % 3]);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            commands.next();
        }
        EXPECT_TRUE(fence.wait());
        EXPECT_EQ(fence.data_->fences, handles);
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, FailedUpdatesAndCancellationAllowTheSameSlotToBeUsedAgain) {
        auto commands = make_commands(2);
        t::TypedFrameResources<FrameValue> frames(commands.manager(), [](size_t slot) { return FrameValue{slot}; });
        vsl::Completion cancelled;
        {
            auto phase = commands.start_compute();
            EXPECT_THROW(commands.next(), std::logic_error);
            EXPECT_THROW(frames.update(phase, [](FrameValue&) { throw std::runtime_error("update"); }), std::runtime_error);
            cancelled = phase.completion();
            phase.cancel();
        }
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
        auto phase = commands.start_compute();
        EXPECT_EQ(phase.frame_slot, 0u);
        frames.update(phase, [](FrameValue& value) { value.updates = 7; });
        EXPECT_EQ(frames.get(phase).updates, 7);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        commands.next();
        EXPECT_EQ(commands.manager().get_buffer().get_current_buffer_idx(), 1u);
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, ImportPreservesTheManagerAndFenceOwnersUsedByExistingFrameResources) {
        auto original = make_commands(2);
        t::TypedFrameResources<FrameValue> frames(original.manager(), [](size_t slot) { return FrameValue{slot}; });
        t::TypedFrameCommands imported(original.manager(), original.fence());
        EXPECT_EQ(imported.manager().data_, original.manager().data_);
        EXPECT_EQ(imported.fence().data_, original.fence().data_);
        auto phase = imported.start_compute();
        EXPECT_NO_THROW(frames.get(phase));
        phase.cancel();
        auto other_manager = vsl::CommandManager::FromBuffer(original.manager().get_buffer());
        auto other = other_manager.start_phase<vsl::ComputePhase>(std::nullopt, std::nullopt, original.fence());
        EXPECT_THROW(frames.get(other), std::invalid_argument);
        other.cancel();
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, APhaseKeepsResourcesAfterFacadeDestructionAndCompletionReleasesFrameData) {
        std::weak_ptr<void> root;
        std::weak_ptr<int> value;
        vsl::Completion result;
        {
            auto commands = std::make_unique<t::TypedFrameCommands>(make_commands(1));
            root = commands->fence().data_;
            using Frames = t::TypedFrameResources<TrackedFrame, vsl::ResourceRetention::COMPLETION>;
            std::optional<Frames> frames;
            frames.emplace(commands->manager(), [&](size_t) {
                auto owner = std::make_shared<int>(42);
                value = owner;
                return TrackedFrame{std::move(owner)};
            });
            auto phase = commands->start_compute();
            EXPECT_EQ(*frames->get(phase).value, 42);
            frames.reset();
            commands.reset();
            EXPECT_FALSE(root.expired());
            EXPECT_FALSE(value.expired());
            result = phase.completion();
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(result.wait());
            EXPECT_TRUE(value.expired());
            EXPECT_FALSE(root.expired());
        }
        EXPECT_TRUE(root.expired());
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, CancelledResultsDoNotRetainTheExecutionArena) {
        std::weak_ptr<void> root;
        vsl::Completion result;
        {
            auto commands = std::make_unique<t::TypedFrameCommands>(make_commands(1));
            root = commands->fence().data_;
            auto phase = commands->start_compute();
            commands.reset();
            result = phase.completion();
            phase.cancel();
            EXPECT_FALSE(root.expired());
        }
        EXPECT_TRUE(root.expired());
        EXPECT_TRUE(result.data_->work.expired());
        EXPECT_EQ(result.status(), vsl::Completion::Status::CANCELLED);
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, MoveAssignmentPublishesTheWholeReplacementBeforeOldDomainCallbacks) {
        auto commands = make_commands(1);
        auto replacement = make_commands(2);
        const auto expected = replacement.fence().data_;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            EXPECT_EQ(commands.frame_count(), 2u);
            EXPECT_EQ(commands.fence().data_, expected);
            auto phase = commands.start_compute();
            phase.cancel();
        });
        {
            auto phase = commands.start_compute();
            commands.manager().get_buffer().retain(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_FALSE(invoked);
        commands = std::move(replacement);
        EXPECT_TRUE(invoked);
        EXPECT_THROW(replacement.manager(), std::invalid_argument);
        EXPECT_THROW(replacement.start_compute(), std::invalid_argument);
    }
    TEST_P(TypedFrameCommandsVulkanIntegrationTest, InvalidCountsOwnersAndRingReplacementAreRejectedWithoutMutation) {
        EXPECT_THROW(make_commands(0), std::invalid_argument);
        auto commands = make_commands(3);
        auto manager = commands.manager();
        const auto original = manager.get_buffer();
        vsl::SynchroManager synchronization(device);
        const auto short_fence = synchronization.create_fence("short");
        EXPECT_THROW((t::TypedFrameCommands(manager, short_fence)), std::invalid_argument);
        const auto canonical = commands.fence();
        vsl::CommandManager borrowed({std::shared_ptr<void>{}, manager.data_.get()});
        EXPECT_THROW((t::TypedFrameCommands(borrowed, canonical)), std::invalid_argument);
        EXPECT_THROW((t::TypedFrameResources<FrameValue>(borrowed, [](size_t slot) { return FrameValue{slot}; })), std::invalid_argument);
        const vsl::FenceHolder forged{{std::make_shared<int>(0), canonical.data_.get()}};
        EXPECT_THROW((t::TypedFrameCommands(manager, forged)), std::invalid_argument);
        EXPECT_THROW((t::TypedFrameCommands(vsl::CommandManager(nullptr), canonical)), std::invalid_argument);
        vsl::DeviceCreateInfo info;
        info.graphics_family = queue_family;
        auto foreign = vsl::LogicalDeviceAccessor::Create(physical, std::move(info));
        auto foreign_fence = vsl::SynchroManager(foreign).create_fence("foreign", size_t{3}, false);
        EXPECT_THROW((t::TypedFrameCommands(manager, foreign_fence)), std::invalid_argument);
        auto replacement = manager.make_exclusive_buffer();
        manager.data_->command_buffer = replacement.data_;
        EXPECT_THROW(commands.manager(), std::invalid_argument);
        EXPECT_THROW(commands.start_compute(), std::invalid_argument);
        EXPECT_THROW(commands.next(), std::invalid_argument);
        manager.data_->command_buffer = original.data_;
        const auto pool = manager.get_pool();
        manager.data_->command_pool = vsl::CommandPool(device).data_;
        EXPECT_THROW(commands.start_compute(), std::invalid_argument);
        EXPECT_THROW((t::TypedFrameCommands(manager, canonical)), std::invalid_argument);
        manager.data_->command_pool = pool.data_;
        auto phase = commands.start_compute();
        EXPECT_EQ(phase.frame_slot, 0u);
        phase.cancel();
    }
    INSTANTIATE_TEST_SUITE_P(StoragePolicies, TypedFrameCommandsVulkanIntegrationTest, testing::Bool());
} // namespace
