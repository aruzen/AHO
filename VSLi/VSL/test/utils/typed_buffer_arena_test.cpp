#include <VSL/lifetime/command_domain.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/utils/typed_buffer_arena.hpp>
#include <VSL/lifetime/buffer_storage.hpp>
#include <VSL/lifetime/buffer_access.hpp>

namespace {
    namespace t = vsl::typed;
    namespace s = vsl::storage;
    constexpr auto USAGE = vsl::MemoryType::TRANSFER_SOURCE | vsl::MemoryType::TRANSFER_DESTINATION;
    using Arena = t::TypedBufferArena<t::BufferResource<t::ResourceId{0}, std::uint32_t, USAGE>,
                                      t::BufferResource<t::ResourceId{1}, std::uint32_t, USAGE>>;
    using View = t::PlacedBufferView<std::uint32_t, USAGE>;
    using Native = vsl::Buffer<USAGE, vsl::MemoryProperty::HOST_VISIBLE>;
    constexpr Arena::Plan PLAN({4, 4});
    static_assert(PLAN.BYTES == 2 * s::BufferLayout::STRIDE);
    static_assert(PLAN.Offset(1) % PLAN.ALIGNMENT == 0);
    static_assert(PLAN.Index<t::ResourceId{1}>() == 1);

    class BufferArenaVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    TEST(BufferArenaLayoutTest, CheckedOffsetsCountsAlignmentAndEmptyPlans) {
        EXPECT_THROW(s::BufferLayout::Offset(SIZE_MAX), std::overflow_error);
        EXPECT_THROW(s::checked_align(SIZE_MAX, 8), std::overflow_error);
        EXPECT_THROW(s::checked_align(4, 3), std::invalid_argument);
        EXPECT_EQ(s::checked_align(9, 8), 16u);
        EXPECT_THROW(Arena::Plan((std::array<size_t, 2>{0, 1})), vsl::exceptions::RuntimeException);
        EXPECT_THROW(Arena::Plan((std::array<size_t, 2>{SIZE_MAX, 1})), vsl::exceptions::RuntimeException);
        EXPECT_THROW(s::ArenaMemory(32, 0), std::invalid_argument);
        alignas(s::BufferLayout::ALIGNMENT) std::array<std::byte, 64> region{};
        auto unowned = std::shared_ptr<void>(std::shared_ptr<void>{}, region.data());
        EXPECT_THROW(s::ArenaMemory(region, s::BufferLayout::ALIGNMENT, unowned), std::invalid_argument);
        EXPECT_THROW(vsl::lifetime::detail::BufferAccess::View(vsl::BufferRef{}, unowned, 0, 1), std::invalid_argument);
        t::BufferArenaPlan<> empty({});
        EXPECT_EQ(empty.BYTES, 0u);
        EXPECT_EQ(empty.ALIGNMENT, 1u);
        EXPECT_TRUE(empty.buffers.empty());
    }

    TEST_F(BufferArenaVulkanIntegrationTest, SameImplTypesOccupyDistinctDirectSlotsAndKeepIndependentValues) {
        Arena arena(device, PLAN);
        auto first = arena.view<t::ResourceId{0}>();
        auto second = arena.view<t::ResourceId{1}>();
        const auto a = first.backend().ref();
        const auto b = second.backend().ref();
        const auto* first_impl = vsl::lifetime::detail::BufferAccess::Impl(a);
        const auto* second_impl = vsl::lifetime::detail::BufferAccess::Impl(b);
        EXPECT_NE(a.identity(), b.identity());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second_impl) - reinterpret_cast<std::uintptr_t>(first_impl), PLAN.Offset(1));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first_impl) % s::BUFFER_IMPL_ALIGNMENT, 0u);
        EXPECT_NE(&a.use(), &b.use());
        first.copy(std::array<std::uint32_t, 4>{1, 2, 3, 4});
        second.copy(std::array<std::uint32_t, 4>{5, 6, 7, 8});
        std::array<std::uint32_t, 4> readback{};
        first.read_into(readback);
        EXPECT_EQ(readback[0], 1u);
        second.read_into(readback);
        EXPECT_EQ(readback[0], 5u);
        EXPECT_THROW(arena.storage().borrow(2), std::out_of_range);
        EXPECT_THROW(first.copy(std::array<std::uint32_t, 1>{0}), std::invalid_argument);
        EXPECT_THROW(first.view(4, 1), std::out_of_range);
    }

    TEST_F(BufferArenaVulkanIntegrationTest, RegionCapacityAlignmentAndExclusiveConstructionAreEnforced) {
        s::ArenaMemory too_small(PLAN.BYTES - 1, PLAN.ALIGNMENT);
        EXPECT_THROW(Arena(device, PLAN, too_small), std::invalid_argument);
        s::ArenaMemory weak_alignment(PLAN.BYTES, 1);
        EXPECT_THROW(Arena(device, PLAN, weak_alignment), std::invalid_argument);
        auto external = std::make_shared<std::array<std::byte, 1024>>();
        EXPECT_THROW(s::ArenaMemory(std::span(*external).subspan(1), PLAN.ALIGNMENT, external), std::invalid_argument);
        s::ArenaMemory memory(PLAN.BYTES, PLAN.ALIGNMENT);
        std::optional<View> retained;
        {
            Arena arena(device, PLAN, memory);
            EXPECT_THROW(Arena(device, PLAN, memory), std::invalid_argument);
            retained.emplace(arena.view<t::ResourceId{0}>());
        }
        EXPECT_THROW(Arena(device, PLAN, memory), std::invalid_argument);
        retained.reset();
        EXPECT_NO_THROW(Arena(device, PLAN, memory));
    }

    TEST_F(BufferArenaVulkanIntegrationTest, NativeInitializationFailureRollsBackAndAllowsRegionReuse) {
        s::ArenaMemory memory(PLAN.BYTES, PLAN.ALIGNMENT);
        auto bad = PLAN.buffers;
        bad[1].properties = vsl::MemoryProperty::HOST_VISIBLE | static_cast<vsl::MemoryProperty>(0x80000000u);
        EXPECT_THROW(s::BufferArena(device, bad, memory), vsl::exceptions::RuntimeException);
        EXPECT_NO_THROW(s::BufferArena(device, PLAN.buffers, memory));
        EXPECT_EQ(sizeof(vsl::impl::BufferImpl), s::BUFFER_IMPL_SIZE);
        EXPECT_EQ(alignof(vsl::impl::BufferImpl), s::BUFFER_IMPL_ALIGNMENT);
    }

    TEST_F(BufferArenaVulkanIntegrationTest, ViewsAndCommandsKeepTheArenaUntilGpuCompletionThenReleaseIt) {
        auto manager = make_manager();
        Native destination(device, 8);
        auto result = View::FromBuffer(destination);
        std::optional<vsl::command::CopyBuffer> command;
        std::weak_ptr<void> owner;
        vsl::BufferRef borrowed;
        {
            Arena arena(device, PLAN);
            owner = arena.storage().owner();
            auto view = arena.view<t::ResourceId{0}>().view(1, 2);
            view.copy(std::array<std::uint32_t, 2>{42, 73});
            borrowed = view.backend().ref();
            // 格納pointerがnullでも、control blockを持つtokenは所有境界として有効。
            auto owner_token = std::shared_ptr<void>(view.backend().owner(), nullptr);
            View source(vsl::lifetime::detail::BufferAccess::View(borrowed, owner_token, view.backend().byte_offset(), view.backend().size()));
            command.emplace(source.copy_to(result));
        }
        ASSERT_FALSE(owner.expired());
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << *command;
        command.reset();
        ASSERT_FALSE(owner.expired());
        EXPECT_THROW(borrowed.write(std::array<std::byte, 4>{}), std::logic_error);
        auto completion = phase.completion();
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(completion.wait());
        EXPECT_TRUE(owner.expired());
        std::array<std::uint32_t, 2> actual{};
        result.read_into(actual);
        EXPECT_EQ(actual, (std::array<std::uint32_t, 2>{42, 73}));
        EXPECT_EQ(completion.status(), vsl::Completion::Status::COMPLETE);
    }

    TEST_F(BufferArenaVulkanIntegrationTest, CancelReleasesTheRegionWhileCompletionObservationSurvives) {
        auto manager = make_manager();
        Native destination(device, 16);
        std::optional<vsl::command::CopyBuffer> command;
        std::weak_ptr<void> owner;
        {
            Arena arena(device, PLAN);
            owner = arena.storage().owner();
            arena.view<t::ResourceId{0}>().copy(std::array<std::uint32_t, 4>{1, 2, 3, 4});
            command.emplace(arena.view<t::ResourceId{0}>().copy_to(View::FromBuffer(destination)));
        }
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << *command;
        command.reset();
        auto completion = phase.completion();
        ASSERT_FALSE(owner.expired());
        phase.cancel();
        EXPECT_TRUE(owner.expired());
        EXPECT_EQ(completion.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_FALSE(completion.wait());
    }

    TEST_F(BufferArenaVulkanIntegrationTest, SameArenaCopyDeduplicatesRetentionAndEmptyArenaAllocatesNoSlots) {
        auto manager = make_manager();
        Arena arena(device, PLAN);
        auto first = arena.view<t::ResourceId{0}>();
        first.copy(std::array<std::uint32_t, 4>{9, 8, 7, 6});
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << first.copy_to(arena.view<t::ResourceId{1}>());
        ASSERT_EQ(manager.get_buffer().data_->recording_context->work->completion_resources.size(), 1u);
        ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        ASSERT_TRUE(phase.completion().wait());
        std::array<std::uint32_t, 4> readback{};
        arena.view<t::ResourceId{1}>().read_into(readback);
        EXPECT_EQ(readback[0], 9u);
        s::BufferArena empty(device, {});
        EXPECT_EQ(empty.size(), 0u);
        EXPECT_THROW(empty.borrow(0), std::out_of_range);
    }

    TEST_F(BufferArenaVulkanIntegrationTest, ManagerKeepsResourcesWhenTheCallerDropsItsCompletion) {
        auto manager = make_manager();
        Native destination(device, 16);
        std::weak_ptr<void> owner;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            {
                Arena arena(device, PLAN);
                owner = arena.storage().owner();
                arena.view<t::ResourceId{0}>().copy(std::array<std::uint32_t, 4>{1, 2, 3, 4});
                phase << arena.view<t::ResourceId{0}>().copy_to(View::FromBuffer(destination));
            }
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        ASSERT_FALSE(owner.expired());
        auto next = manager.start_phase<vsl::ComputePhase>();
        EXPECT_TRUE(owner.expired());
        next.cancel();
    }

    TEST_F(BufferArenaVulkanIntegrationTest, ExceptionCancellationReturnsExternallyOwnedCpuMemory) {
        auto manager = make_manager();
        Native destination(device, 16);
        auto releases = std::make_shared<size_t>(0);
        EXPECT_THROW(([&] {
                         auto phase = manager.start_phase<vsl::ComputePhase>();
                         {
                             auto* bytes = static_cast<std::byte*>(::operator new(PLAN.BYTES, std::align_val_t(PLAN.ALIGNMENT)));
                             auto owner = std::shared_ptr<void>(bytes, [releases](void* pointer) {
                                 ++*releases;
                                 ::operator delete(pointer, std::align_val_t(PLAN.ALIGNMENT));
                             });
                             s::ArenaMemory memory(std::span(bytes, PLAN.BYTES), PLAN.ALIGNMENT, owner);
                             Arena arena(device, PLAN, memory);
                             arena.view<t::ResourceId{0}>().copy(std::array<std::uint32_t, 4>{1, 2, 3, 4});
                             phase << arena.view<t::ResourceId{0}>().copy_to(View::FromBuffer(destination));
                         }
                         EXPECT_EQ(*releases, 0u);
                         throw std::runtime_error("Cancel recorded copy");
                     }()),
                     std::runtime_error);
        EXPECT_EQ(*releases, 1u);
    }
} // namespace
