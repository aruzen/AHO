#include <VSL/lifetime/synchronization_access.hpp>
#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/resource_arena.hpp>
#include <VSL/lifetime/synchronization_storage.hpp>
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/utils/typed_buffer_arena.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::SynchronizationAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    namespace t = vsl::typed;
    namespace s = vsl::storage;
    namespace l = vsl::lifetime;
    constexpr std::array<t::ResourceRequest, 4> REQUESTS{{{{1}, s::ResourceKind::SEMAPHORE}, {{2}, s::ResourceKind::SEMAPHORE}, {{3}, s::ResourceKind::FENCE}, {{4}, s::ResourceKind::FENCE}}};
    constexpr t::ResourcePlan<4> PLAN(REQUESTS);
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    constexpr vsl::BufferCreateInfo BUFFER_INFO{.bytes = 16, .usage = USAGE, .properties = vsl::MemoryProperty::HOST_VISIBLE};
    class SynchronizationArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        std::array<l::ResourceInit, 4> inputs() const { return {vsl::SemaphoreCreateInfo{1}, vsl::SemaphoreCreateInfo{2}, vsl::FenceCreateInfo{1, false}, vsl::FenceCreateInfo{2, true}}; }
    };
    TEST(SynchronizationPlacementTest, ExactLayoutsIncludeContextsAndRejectInsufficientRegions) {
        static_assert(!std::is_copy_constructible_v<vsl::impl::SemaphoreImpl> && !std::is_move_constructible_v<vsl::impl::SemaphoreImpl>);
        static_assert(!std::is_copy_constructible_v<vsl::impl::FenceImpl> && !std::is_move_constructible_v<vsl::impl::FenceImpl>);
        static_assert(sizeof(vsl::impl::SemaphoreImpl) == s::SEMAPHORE_IMPL_SIZE && alignof(vsl::impl::SemaphoreImpl) == s::SEMAPHORE_IMPL_ALIGNMENT);
        static_assert(sizeof(vsl::impl::FenceImpl) == s::FENCE_IMPL_SIZE && alignof(vsl::impl::FenceImpl) == s::FENCE_IMPL_ALIGNMENT);
        static_assert(sizeof(l::SemaphoreSlotStorage) == s::SEMAPHORE_SLOT_SIZE && alignof(l::SemaphoreSlotStorage) == s::SEMAPHORE_SLOT_ALIGNMENT);
        static_assert(sizeof(l::FenceSlotStorage) == s::FENCE_SLOT_SIZE && alignof(l::FenceSlotStorage) == s::FENCE_SLOT_ALIGNMENT);
        EXPECT_EQ(PLAN.bytes(), 2 * s::SEMAPHORE_SLOT_SIZE + 2 * s::FENCE_SLOT_SIZE);
        EXPECT_GT(s::SEMAPHORE_SLOT_SIZE, s::SEMAPHORE_IMPL_SIZE);
        EXPECT_GT(s::FENCE_SLOT_SIZE, s::FENCE_IMPL_SIZE);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes() - 1, PLAN.alignment()), std::invalid_argument);
        EXPECT_THROW(PLAN.require_region(PLAN.bytes(), PLAN.alignment() / 2), std::invalid_argument);
        EXPECT_THROW(s::checked_add(std::numeric_limits<size_t>::max(), s::FENCE_SLOT_SIZE), std::overflow_error);
    }
    TEST(SynchronizationPlacementTest, EmptyHoldersRejectOwningGetters) {
        vsl::SemaphoreHolder semaphore;
        vsl::FenceHolder fence;
        EXPECT_EQ(semaphore.size(), 0u);
        EXPECT_EQ(fence.size(), 0u);
        EXPECT_THROW(semaphore.owner_ref(), std::invalid_argument);
        EXPECT_THROW(fence.owner_ref(), std::invalid_argument);
        EXPECT_THROW(semaphore.get_device(), std::invalid_argument);
        EXPECT_THROW(fence.get_device(), std::invalid_argument);
        EXPECT_NO_THROW(semaphore.destroy());
        EXPECT_NO_THROW(fence.destroy());
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, SameTypeSlotsAreDirectDistinctAndShareCanonicalOwnership) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto first = arena.semaphore({1}), second = arena.semaphore({2});
        auto fence = arena.fence({3}), signaled = arena.fence({4});
        const auto base = reinterpret_cast<std::uintptr_t>(SynchronizationAccess::Owned(first).get());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(SynchronizationAccess::Owned(second).get()) - base, PLAN.placement({2}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(SynchronizationAccess::Owned(fence).get()) - base, PLAN.placement({3}).offset);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(SynchronizationAccess::Owned(signaled).get()) - base, PLAN.placement({4}).offset);
        EXPECT_EQ(base % s::SEMAPHORE_SLOT_ALIGNMENT, 0u);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(SynchronizationAccess::Owned(fence).get()) % s::FENCE_SLOT_ALIGNMENT, 0u);
        EXPECT_EQ(first.size(), 1u);
        EXPECT_EQ(second.size(), 2u);
        EXPECT_EQ(fence.size(), 1u);
        EXPECT_EQ(signaled.size(), 2u);
        EXPECT_FALSE(SynchronizationAccess::Owned(first).owner_before(SynchronizationAccess::Owned(fence)));
        EXPECT_FALSE(SynchronizationAccess::Owned(fence).owner_before(SynchronizationAccess::Owned(first)));
        EXPECT_NE(SynchronizationAccess::Owned(first)->semaphores[0], SynchronizationAccess::Owned(second)->semaphores[0]);
        EXPECT_FALSE(fence.wait(std::uint64_t{0}));
        EXPECT_TRUE(signaled.wait(std::uint64_t{0}));
        signaled.reset(1);
        EXPECT_TRUE(signaled.wait(0, 0));
        EXPECT_FALSE(signaled.wait(1, 0));
        EXPECT_EQ(first.get_device(), device);
        EXPECT_EQ(fence.get_device(), device);
        EXPECT_TRUE(first.owner_ref().managed());
        EXPECT_TRUE(fence.owner_ref().managed());
        EXPECT_THROW(arena.fence({1}), std::invalid_argument);
        EXPECT_THROW(arena.semaphore({99}), std::out_of_range);
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, LaterConstructionFailureRollsBackAndAllowsTheSameRegionToBeReused) {
        s::ArenaMemory memory(PLAN.bytes(), PLAN.alignment());
        for (const size_t index : {size_t{1}, size_t{3}}) {
            auto invalid = inputs();
            if (index == 1)
                invalid[index] = vsl::SemaphoreCreateInfo{0};
            else
                invalid[index] = vsl::FenceCreateInfo{0, false};
            EXPECT_THROW((l::ResourceArena(device, PLAN, invalid, memory)), std::invalid_argument);
            l::ResourceArena retry(device, PLAN, inputs(), memory);
            EXPECT_EQ(retry.semaphore({2}).size(), 2u);
            EXPECT_TRUE(retry.fence({4}).wait(std::uint64_t{0}));
        }
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, BackendRejectsReinitializationBeforeChangingHandlesOrFenceState) {
        l::ResourceArena arena(device, PLAN, inputs());
        const auto semaphore = arena.semaphore({1});
        auto fence = arena.fence({4});
        const auto semaphores = SynchronizationAccess::Owned(semaphore)->semaphores;
        const auto fences = SynchronizationAccess::Owned(fence)->fences;
        EXPECT_THROW(vsl::operation::initialize_semaphore(*SynchronizationAccess::Owned(semaphore), *device.data_, {3}), std::logic_error);
        EXPECT_THROW(vsl::operation::initialize_fence(*SynchronizationAccess::Owned(fence), *device.data_, {3, false}), std::logic_error);
        EXPECT_EQ(SynchronizationAccess::Owned(semaphore)->semaphores, semaphores);
        EXPECT_EQ(SynchronizationAccess::Owned(fence)->fences, fences);
        EXPECT_EQ(SynchronizationAccess::Owned(fence)->uses.size(), 2u);
        EXPECT_TRUE(fence.wait(std::uint64_t{0}));
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, ImportedHoldersKeepTheOriginalOwnerAndPhysicalUseState) {
        constexpr std::array<t::ResourceRequest, 2> requests{{{{10}, s::ResourceKind::SEMAPHORE, true}, {{20}, s::ResourceKind::FENCE, true}}};
        constexpr t::ResourcePlan<2> plan(requests);
        std::optional<l::ResourceArena> imported;
        std::weak_ptr<void> original, importer;
        vsl::SemaphoreHolder semaphore;
        vsl::FenceHolder fence;
        {
            l::ResourceArena source(device, PLAN, inputs());
            original = source.owner();
            const std::array<l::ResourceInit, 2> init{source.semaphore({1}), source.fence({3})};
            imported.emplace(device, plan, init);
            importer = imported->owner();
            semaphore = imported->semaphore({10});
            fence = imported->fence({20});
            EXPECT_EQ(SynchronizationAccess::Owned(semaphore), SynchronizationAccess::Owned(source.semaphore({1})));
            EXPECT_EQ(SynchronizationAccess::Owned(fence), SynchronizationAccess::Owned(source.fence({3})));
            EXPECT_FALSE(SynchronizationAccess::Owned(semaphore).owner_before(source.owner()));
            EXPECT_FALSE(source.owner().owner_before(SynchronizationAccess::Owned(semaphore).share()));
            auto commands = make_manager().independent(1);
            auto phase = commands.start_phase<vsl::ComputePhase>(std::nullopt, semaphore, fence);
            EXPECT_THROW(source.semaphore({1}).destroy(), std::logic_error);
            EXPECT_THROW(source.fence({3}).reset(), std::logic_error);
            phase.cancel();
        }
        imported.reset();
        EXPECT_TRUE(importer.expired());
        EXPECT_FALSE(original.expired());
        fence.destroy();
        semaphore.destroy();
        fence = {};
        semaphore = {};
        EXPECT_TRUE(original.expired());
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, ImportsRejectForeignOwnersAndDestroyedNativeResources) {
        constexpr std::array<t::ResourceRequest, 2> requests{{{{10}, s::ResourceKind::SEMAPHORE, true}, {{20}, s::ResourceKind::FENCE, true}}};
        constexpr t::ResourcePlan<2> plan(requests);
        l::ResourceArena arena(device, PLAN, inputs());
        const auto semaphore = arena.semaphore({1});
        const auto fence = arena.fence({3});
        vsl::SemaphoreHolder forged_semaphore;
        SynchronizationAccess::Owned(forged_semaphore) = std::shared_ptr<vsl::impl::SemaphoreImpl>(std::make_shared<int>(0), SynchronizationAccess::Owned(semaphore).get());
        vsl::FenceHolder forged_fence;
        SynchronizationAccess::Owned(forged_fence) = std::shared_ptr<vsl::impl::FenceImpl>(std::make_shared<int>(0), SynchronizationAccess::Owned(fence).get());
        EXPECT_THROW(forged_semaphore.owner_ref(), std::invalid_argument);
        EXPECT_THROW(forged_fence.get_device(), std::invalid_argument);
        const std::array<l::ResourceInit, 2> bad_semaphore{forged_semaphore, fence}, bad_fence{semaphore, forged_fence};
        EXPECT_THROW((l::ResourceArena(device, plan, bad_semaphore)), std::invalid_argument);
        EXPECT_THROW((l::ResourceArena(device, plan, bad_fence)), std::invalid_argument);
        auto destroyed = fence;
        destroyed.destroy();
        const std::array<l::ResourceInit, 2> invalid{semaphore, destroyed};
        EXPECT_THROW((l::ResourceArena(device, plan, invalid)), std::invalid_argument);
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, HeapHoldersPublishOnlyTheOriginalDeviceDependency) {
        vsl::LogicalDeviceAccessor saved;
        std::weak_ptr<void> semaphore_owner, fence_owner;
        {
            vsl::SynchroManager manager(device);
            auto semaphore = manager.create_semaphore("heap");
            auto fence = manager.create_fence("heap", true);
            semaphore_owner = SynchronizationAccess::Owned(semaphore).share();
            fence_owner = SynchronizationAccess::Owned(fence).share();
            EXPECT_TRUE(semaphore.owner_ref().managed());
            EXPECT_TRUE(fence.owner_ref().managed());
            EXPECT_EQ(semaphore.get_device(), device);
            saved = fence.get_device();
            for (const auto* node : {SynchronizationAccess::Owned(semaphore)->owner_node, SynchronizationAccess::Owned(fence)->owner_node}) {
                std::vector<l::OwnerRef> parents;
                node->visit_edges(+[](void* context, const l::OwnerRef& parent) { static_cast<std::vector<l::OwnerRef>*>(context)->push_back(parent); }, &parents);
                ASSERT_EQ(parents.size(), 1u);
                EXPECT_FALSE(parents[0].share().owner_before(device.data_));
                EXPECT_FALSE(device.data_.owner_before(parents[0].share()));
            }
        }
        EXPECT_TRUE(semaphore_owner.expired());
        EXPECT_TRUE(fence_owner.expired());
        EXPECT_EQ(saved, device);
        EXPECT_FALSE(saved.data_.owner_before(device.data_));
        EXPECT_FALSE(device.data_.owner_before(saved.data_));
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, ExplicitDestroyDisarmsNativeResourcesButKeepsConstructedSlotsUntilRootRelease) {
        std::weak_ptr<void> root;
        vsl::FenceHolder survivor;
        {
            l::ResourceArena arena(device, PLAN, inputs());
            root = arena.owner();
            auto semaphore = arena.semaphore({1});
            auto fence = arena.fence({4});
            auto* context = SynchronizationAccess::Owned(fence)->owner_context;
            survivor = fence;
            semaphore.destroy();
            fence.destroy();
            EXPECT_EQ(arena.semaphore({1}).size(), 0u);
            EXPECT_EQ(arena.fence({4}).size(), 0u);
            EXPECT_EQ(SynchronizationAccess::Owned(fence)->uses.size(), 2u);
            EXPECT_EQ(SynchronizationAccess::Owned(fence)->owner_context, context);
            EXPECT_THROW(fence.wait(), std::logic_error);
            EXPECT_THROW(semaphore.get_device(), std::invalid_argument);
            EXPECT_NO_THROW(fence.destroy());
            EXPECT_NO_THROW(semaphore.destroy());
        }
        EXPECT_FALSE(root.expired());
        survivor = {};
        EXPECT_TRUE(root.expired());
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, DistinctSynchronizationSlotsUseOneWorkOwnerAndSurviveWrappersThroughTypedCopy) {
        constexpr std::array<t::ResourceRequest, 8> requests{{{{1}, s::ResourceKind::SEMAPHORE}, {{2}, s::ResourceKind::SEMAPHORE}, {{3}, s::ResourceKind::FENCE}, {{4}, s::ResourceKind::COMMAND_POOL}, {{5}, s::ResourceKind::COMMAND_BUFFER}, {{6}, s::ResourceKind::COMMAND_BUFFER}, {{7}, s::ResourceKind::BUFFER}, {{8}, s::ResourceKind::BUFFER}}};
        constexpr t::ResourcePlan<8> plan(requests);
        const std::array<l::ResourceInit, 8> init{vsl::SemaphoreCreateInfo{}, vsl::SemaphoreCreateInfo{}, vsl::FenceCreateInfo{}, vsl::CommandPoolCreateInfo{}, l::CommandBufferInit{t::ResourceId{4}, 1}, l::CommandBufferInit{t::ResourceId{4}, 1}, BUFFER_INFO, BUFFER_INFO};
        std::optional<vsl::CommandManager> producer, consumer;
        std::optional<vsl::command::CopyBuffer> command;
        vsl::SemaphoreHolder wait, signal;
        vsl::FenceHolder fence;
        std::weak_ptr<void> root;
        vsl::BufferRef destination;
        const std::array<std::uint32_t, 4> expected{3, 9, 27, 81};
        {
            l::ResourceArena arena(device, plan, init);
            root = arena.owner();
            producer = vsl::CommandManager::FromBuffer(arena.command_buffer({5}));
            consumer = vsl::CommandManager::FromBuffer(arena.command_buffer({6}));
            wait = arena.semaphore({1});
            signal = arena.semaphore({2});
            fence = arena.fence({3});
            View source(arena.buffer({7})), target(arena.buffer({8}));
            source.copy(expected);
            destination = target.backend().ref();
            command.emplace(source.copy_to(target));
        }
        vsl::Completion result;
        {
            auto first = producer->start_phase<vsl::ComputePhase>(std::nullopt, wait);
            auto second = consumer->start_phase<vsl::ComputePhase>(wait, signal, fence);
            // producer失敗時に、未signalのwaitを持つconsumerを自動送信させない。
            const auto cancel = +[](vsl::ComputePhase* phase) { phase->cancel(); };
            std::unique_ptr<vsl::ComputePhase, decltype(cancel)> cancel_consumer(&second, cancel);
            const auto* work = CommandAccess::Data(consumer->get_buffer())->recording_context->work;
            EXPECT_NE(work->wait_semaphore, work->signal_semaphore);
            EXPECT_EQ(work->wait_semaphore, SynchronizationAccess::Owned(wait).get());
            EXPECT_EQ(work->signal_semaphore, SynchronizationAccess::Owned(signal).get());
            EXPECT_EQ(work->external_fence, SynchronizationAccess::Owned(fence).get());
            EXPECT_TRUE(work->resources.empty());
            EXPECT_EQ(work->completion_resources.size(), 1u);
            EXPECT_EQ(work->synchronization_owners[0], work->synchronization_owners[1]);
            EXPECT_EQ(work->synchronization_owners[1], work->synchronization_owners[2]);
            EXPECT_EQ(SynchronizationAccess::Owned(wait)->use.graphics.size(), 2u);
            EXPECT_EQ(SynchronizationAccess::Owned(signal)->use.graphics.size(), 1u);
            EXPECT_THROW(fence.reset(), std::logic_error);
            EXPECT_THROW(wait.destroy(), std::logic_error);
            second << *command;
            command.reset();
            producer.reset();
            consumer.reset();
            wait = {};
            signal = {};
            fence = {};
            EXPECT_FALSE(root.expired());
            ASSERT_EQ(first.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            (void)cancel_consumer.release();
            ASSERT_EQ(second.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            result = second.completion();
            ASSERT_TRUE(result.wait());
            std::array<std::uint32_t, 4> actual{};
            destination.read_into(std::as_writable_bytes(std::span(actual)));
            EXPECT_EQ(actual, expected);
        }
        EXPECT_TRUE(root.expired());
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
    }
    TEST_F(SynchronizationArenaVulkanIntegrationTest, CancelledWorkReleasesItsArenaWithoutResultOwnershipCycles) {
        std::weak_ptr<void> root;
        vsl::Completion cancelled;
        {
            l::ResourceArena arena(device, PLAN, inputs());
            root = arena.owner();
            auto manager = make_manager().independent(1);
            auto phase = manager.start_phase<vsl::ComputePhase>(arena.semaphore({1}), arena.semaphore({2}), arena.fence({3}));
            cancelled = phase.completion();
            EXPECT_THROW(arena.fence({3}).destroy(), std::logic_error);
            phase.cancel();
            EXPECT_NO_THROW(arena.fence({3}).reset());
        }
        EXPECT_TRUE(root.expired());
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_TRUE(cancelled.data_->work.expired());
    }
} // namespace

namespace {
    TEST_F(SynchronizationArenaVulkanIntegrationTest, ManagedCompositeOwnersCannotReplaceTheCanonicalSynchronizationOwner) {
        struct Composite {
            l::DependencyNode node;
            l::OwnerRef source;
            explicit Composite(l::OwnerRef source) : source(std::move(source)) {}
            ~Composite() { node.retire(); }
            static void Visit(const void* context, l::DependencyNode::EdgeVisitor visitor, void* output) {
                visitor(output, static_cast<const Composite*>(context)->source);
            }
        };
        l::ResourceArena arena(device, PLAN, inputs());
        auto semaphore = arena.semaphore({1});
        auto fence = arena.fence({4});
        auto composite = std::make_shared<Composite>(semaphore.owner_ref());
        composite->node.publish(composite, composite.get(), Composite::Visit);
        const auto owner = composite->node.ref(composite);
        ASSERT_TRUE(owner.managed());
        auto wrong_semaphore = owner.alias(SynchronizationAccess::Owned(semaphore).get());
        auto wrong_fence = owner.alias(SynchronizationAccess::Owned(fence).get());
        EXPECT_THROW(SynchronizationAccess::FromOwned(wrong_semaphore), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromOwned(wrong_fence), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromShared(wrong_semaphore.share()), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromShared(wrong_fence.share()), std::invalid_argument);
        const auto restored_semaphore = SynchronizationAccess::FromOwned(SynchronizationAccess::Owned(semaphore));
        auto restored_fence = SynchronizationAccess::FromOwned(SynchronizationAccess::Owned(fence));
        EXPECT_EQ(restored_semaphore.size(), 1u);
        EXPECT_TRUE(restored_fence.wait(std::uint64_t{0}));
        EXPECT_EQ(&SynchronizationAccess::Owned(restored_semaphore)->use, &SynchronizationAccess::Owned(semaphore)->use);
        EXPECT_EQ(&SynchronizationAccess::Owned(restored_fence)->uses, &SynchronizationAccess::Owned(fence)->uses);
    }
} // namespace

namespace {
    TEST_F(SynchronizationArenaVulkanIntegrationTest, RetiredCanonicalOwnersRejectNewAccessWithoutChangingNativeResources) {
        l::ResourceArena arena(device, PLAN, inputs());
        auto semaphore = arena.semaphore({1});
        auto fence = arena.fence({4});
        const auto semaphore_owner = SynchronizationAccess::Owned(semaphore);
        const auto fence_owner = SynchronizationAccess::Owned(fence);
        const auto native_semaphores = semaphore_owner->semaphores;
        const auto native_fences = fence_owner->fences;
        ASSERT_EQ(semaphore_owner->owner_node, fence_owner->owner_node);
        const_cast<l::DependencyNode*>(semaphore_owner->owner_node)->retire();
        EXPECT_THROW(semaphore.owner_ref(), std::invalid_argument);
        EXPECT_THROW(fence.owner_ref(), std::invalid_argument);
        EXPECT_THROW(semaphore.get_device(), std::invalid_argument);
        EXPECT_THROW(fence.get_device(), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromOwned(semaphore_owner), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromOwned(fence_owner), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromShared(semaphore_owner.share()), std::invalid_argument);
        EXPECT_THROW(SynchronizationAccess::FromShared(fence_owner.share()), std::invalid_argument);
        EXPECT_THROW(fence.wait(std::uint64_t{0}), std::invalid_argument);
        EXPECT_THROW(fence.reset(), std::invalid_argument);
        EXPECT_THROW(fence.destroy(), std::invalid_argument);
        EXPECT_THROW(semaphore.destroy(), std::invalid_argument);
        EXPECT_EQ(semaphore_owner->semaphores, native_semaphores);
        EXPECT_EQ(fence_owner->fences, native_fences);
    }
} // namespace
