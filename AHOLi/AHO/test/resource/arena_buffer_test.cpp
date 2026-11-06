#include <AHO/resource/arena_buffer.hpp>
#include <AHO/resource/typed_buffer.hpp>
#include <AHO/command/buffer_upload.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <array>

namespace {
    static_assert(aho::resource::gpu_buffer<aho::resource::ArenaBuffer>);
    static_assert(std::is_copy_constructible_v<aho::resource::ArenaBuffer>);
    static_assert(!std::is_base_of_v<aho::resource::ImportedBuffer, aho::resource::ArenaBuffer>);
    static_assert(!std::is_convertible_v<aho::resource::ArenaBuffer, aho::resource::ImportedBuffer>);
    static_assert(!std::is_constructible_v<aho::resource::ImportedBuffer, const aho::resource::ArenaBuffer&>);
} // namespace
#ifdef AHO_GPU_TESTS
#include "support/vulkan_fixture.hpp"

namespace {
    namespace r = aho::resource;
    class ArenaBufferVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(ArenaBufferVulkanIntegrationTest, AliasesFollowTheNewGenerationWhileOldSnapshotsKeepTheirContents) {
        auto buffer = r::make_arena_buffer(device);
        EXPECT_EQ(buffer.capacity(), 0u);
        buffer.assign({});
        const std::array<std::uint32_t, 2> initial{11, 13}, changed{17, 19};
        buffer.assign(std::as_bytes(std::span(initial)));
        auto alias = buffer;
        auto old = buffer.binding().snapshot();
        auto old_typed = r::uniform_view<std::uint32_t>(buffer);
        buffer.reserve(64);
        EXPECT_EQ(alias.capacity(), 64u);
        EXPECT_EQ(alias.size(), sizeof(initial));
        const auto current = alias.binding().snapshot();
        EXPECT_NE(old.storage().ref().data, current.storage().ref().data);
        std::array<std::uint32_t, 2> result{};
        alias.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, initial);
        alias.write(std::as_bytes(std::span(changed)));
        old.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, initial);
        buffer.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, changed);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        old_typed.descriptor().use.record(manager.get_buffer());
        EXPECT_FALSE(current.storage().ref().use().busy());
        EXPECT_TRUE(buffer.busy());
        EXPECT_FALSE(buffer.wait(0));
        EXPECT_THROW(buffer.reserve(128), std::logic_error);
        EXPECT_THROW(alias.assign({}), std::logic_error);
        phase.cancel();
        EXPECT_TRUE(buffer.wait(0));
        buffer.resize(128);
        EXPECT_EQ(alias.size(), 128u);
        EXPECT_EQ(alias.capacity(), 128u);
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, AllocationFailureAndInvalidFactoryLeaveTheOldGenerationUnchanged) {
        bool fail = false, undersized = false;
        r::ArenaBuffer buffer(device, 8, [&](auto logical_device, size_t bytes) {
            if (fail)
                throw std::runtime_error("Injected allocation failure");
            const vsl::BufferCreateInfo info{undersized ? 1 : bytes, r::detail::STANDARD_BUFFER_USAGE};
            return vsl::lifetime::BufferArena(logical_device, std::span(&info, 1)).view(0);
        });
        const std::array<std::uint32_t, 2> values{3, 5};
        buffer.write(std::as_bytes(std::span(values)));
        const auto before = buffer.binding().snapshot();
        fail = true;
        EXPECT_THROW(buffer.reserve(64), std::runtime_error);
        EXPECT_THROW(buffer.resize(64), std::runtime_error);
        fail = false;
        undersized = true;
        EXPECT_THROW(buffer.reserve(64), std::invalid_argument);
        const auto after = buffer.binding().snapshot();
        EXPECT_EQ(before.storage().ref().data, after.storage().ref().data);
        EXPECT_EQ(buffer.size(), 8u);
        EXPECT_EQ(buffer.capacity(), 8u);
        std::array<std::uint32_t, 2> result{};
        buffer.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, values);
        undersized = false;
        buffer.reserve(64);
        EXPECT_EQ(buffer.capacity(), 64u);
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, SavedUploadUsesTheCurrentGenerationAtInvokeAndReleasesItAfterCompletion) {
        auto manager = make_manager();
        std::weak_ptr<void> owner;
        vsl::Completion completed;
        r::Buffer<r::BufferType::GPU_FRIENDLY> output(device, 16);
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            completed = phase.completion();
            {
                auto source = r::make_arena_buffer(device, 4);
                auto destination = r::make_arena_buffer(device, 4);
                auto upload = destination.upload(source);
                const std::array<std::uint32_t, 4> values{2, 4, 6, 8};
                source.assign(std::as_bytes(std::span(values)));
                destination.resize(16);
                phase << upload << output.upload(destination);
                owner = destination.binding().owner();
            }
            EXPECT_FALSE(owner.expired());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(completed.wait());
            EXPECT_TRUE(owner.expired());
        }
        std::array<std::uint32_t, 4> result{};
        output.read_into(std::as_writable_bytes(std::span(result)));
        EXPECT_EQ(result, (std::array<std::uint32_t, 4>{2, 4, 6, 8}));
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, CopyFailureDoesNotPublishTheCandidateGeneration) {
        auto manager = make_manager();
        auto pending = manager.start_phase<vsl::ComputePhase>();
        bool block = false;
        std::weak_ptr<void> candidate;
        r::ArenaBuffer buffer(device, 4, [&](auto logical_device, size_t bytes) {
            const vsl::BufferCreateInfo info{bytes, r::detail::STANDARD_BUFFER_USAGE};
            auto view = vsl::lifetime::BufferArena(logical_device, std::span(&info, 1)).view(0);
            if (block) {
                candidate = view.owner();
                view.resource_use().record(manager.get_buffer(), vsl::ResourceRetention::COMPLETION);
            }
            return view;
        });
        const std::uint32_t value = 31;
        buffer.write(std::as_bytes(std::span(&value, 1)));
        block = true;
        EXPECT_THROW(buffer.reserve(64), std::logic_error);
        EXPECT_EQ(buffer.capacity(), 4u);
        EXPECT_EQ(buffer.size(), 4u);
        std::uint32_t result = 0;
        buffer.read_into(std::as_writable_bytes(std::span(&result, 1)));
        EXPECT_EQ(result, value);
        EXPECT_FALSE(candidate.expired());
        pending.cancel();
        EXPECT_TRUE(candidate.expired());
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, SnapshotDoesNotRetainFactoryConfigurationOrTheLatestGeneration) {
        std::weak_ptr<int> context;
        std::optional<r::BufferSnapshot> old;
        std::weak_ptr<void> latest;
        {
            auto token = std::make_shared<int>(7);
            context = token;
            auto buffer = r::make_arena_buffer(device, 4, [token](auto logical_device, size_t bytes) {
                const vsl::BufferCreateInfo info{bytes, r::detail::STANDARD_BUFFER_USAGE};
                return vsl::lifetime::BufferArena(logical_device, std::span(&info, 1)).view(0);
            });
            old.emplace(buffer.binding().snapshot());
            buffer.reserve(32);
            latest = buffer.binding().owner();
        }
        EXPECT_TRUE(latest.expired());
        EXPECT_TRUE(context.expired());
        std::array<std::byte, 4> result{};
        EXPECT_NO_THROW(old->read_into(result));
        old.reset();
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, ExplicitSnapshotImportKeepsItsFixedCapacityAndOldGeneration) {
        auto growing = r::make_arena_buffer(device, 4);
        const std::uint32_t initial = 7, changed = 23;
        growing.write(std::as_bytes(std::span(&initial, 1)));
        const auto snapshot = growing.binding().snapshot();
        auto fixed = r::import_buffer(snapshot.storage());
        auto alias = fixed;
        growing.reserve(64);
        growing.write(std::as_bytes(std::span(&changed, 1)));
        EXPECT_EQ(fixed.capacity(), 4u);
        EXPECT_EQ(alias.capacity(), 4u);
        EXPECT_EQ(growing.capacity(), 64u);
        std::uint32_t result = 0;
        fixed.read_into(std::as_writable_bytes(std::span(&result, 1)));
        EXPECT_EQ(result, initial);
        EXPECT_THROW(fixed.resize(8), std::out_of_range);
        alias.resize(0);
        EXPECT_EQ(fixed.size(), 0u);
        EXPECT_EQ(growing.size(), 4u);
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, SharedArenaSlotsKeepTheirCanonicalOwnerAndRetainItOnce) {
        const std::array<vsl::BufferCreateInfo, 2> infos{{{4, r::detail::STANDARD_BUFFER_USAGE}, {4, r::detail::STANDARD_BUFFER_USAGE}}};
        vsl::lifetime::BufferArena arena(device, infos);
        const std::weak_ptr<void> owner = arena.owner();
        r::ArenaBuffer first(device, 4, [view = arena.view(0)](auto, size_t) { return view; });
        r::ArenaBuffer second(device, 4, [view = arena.view(1)](auto, size_t) { return view; });
        const auto a = first.binding().owner(), b = second.binding().owner();
        EXPECT_FALSE(a.owner_before(owner));
        EXPECT_FALSE(owner.owner_before(a));
        EXPECT_FALSE(a.owner_before(b));
        EXPECT_FALSE(b.owner_before(a));
        const auto first_view = r::uniform_view<std::uint32_t>(first);
        const auto second_view = r::storage_view<std::uint32_t>(second);
        EXPECT_NE(first_view.descriptor().use.state, second_view.descriptor().use.state);
        auto manager = make_manager();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        const auto baseline = owner.use_count();
        phase << second.upload(first);
        EXPECT_EQ(owner.use_count(), baseline + 1);
        first_view.descriptor().use.record(manager.get_buffer(), vsl::ResourceRetention::COMPLETION);
        second_view.descriptor().use.record(manager.get_buffer(), vsl::ResourceRetention::COMPLETION);
        EXPECT_EQ(owner.use_count(), baseline + 1);
        EXPECT_TRUE(first.busy());
        EXPECT_TRUE(second.busy());
        EXPECT_THROW(arena.borrow(0).fill(std::byte{}), std::logic_error);
        EXPECT_THROW(arena.borrow(1).fill(std::byte{}), std::logic_error);
        phase.cancel();
        EXPECT_EQ(owner.use_count(), baseline);
        EXPECT_FALSE(first.busy());
        EXPECT_FALSE(second.busy());
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, ReturnedOwnerKeepsResourceDependenciesAfterTheFactoryIsReleased) {
        struct Storage {
            std::shared_ptr<int> dependency;
            vsl::lifetime::BufferArena arena;
        };
        std::weak_ptr<int> dependency;
        std::optional<r::BufferSnapshot> snapshot;
        {
            auto token = std::make_shared<int>(19);
            dependency = token;
            const vsl::BufferCreateInfo info{4, r::detail::STANDARD_BUFFER_USAGE};
            auto owner = std::make_shared<Storage>(token, vsl::lifetime::BufferArena(device, std::span(&info, 1)));
            auto buffer = r::make_arena_buffer(device, 4, [owner](auto, size_t) {
                return vsl::BufferView(owner->arena.borrow(0), owner, 0, 4);
            });
            snapshot.emplace(buffer.binding().snapshot());
        }
        EXPECT_FALSE(dependency.expired());
        std::array<std::byte, 4> result{};
        EXPECT_NO_THROW(snapshot->read_into(result));
        snapshot.reset();
        EXPECT_TRUE(dependency.expired());
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, FactoryCannotReuseTheCurrentImplOrDropRequiredUsage) {
        const vsl::BufferCreateInfo info{128, r::detail::STANDARD_BUFFER_USAGE};
        auto root = vsl::lifetime::BufferArena(device, std::span(&info, 1)).view(0);
        r::ArenaBuffer reused(device, 8, [root](auto, size_t bytes) { return root.view(0, bytes); });
        EXPECT_THROW(reused.reserve(64), std::invalid_argument);
        EXPECT_EQ(reused.capacity(), 8u);
        bool restricted = false;
        r::ArenaBuffer buffer(device, 8, [&](auto logical_device, size_t bytes) {
            const vsl::BufferCreateInfo create{bytes, restricted ? vsl::MemoryType::TRANSFER_SOURCE : r::detail::STANDARD_BUFFER_USAGE};
            return vsl::lifetime::BufferArena(logical_device, std::span(&create, 1)).view(0);
        });
        restricted = true;
        EXPECT_THROW(buffer.reserve(64), std::invalid_argument);
        EXPECT_EQ(buffer.capacity(), 8u);
    }
    TEST_F(ArenaBufferVulkanIntegrationTest, FullRangeNativeAdapterIsLazyAndSharesThePhysicalUseState) {
        auto buffer = r::make_arena_buffer(device, 16);
        auto binding = buffer.binding();
        const auto snapshot = binding.snapshot();
        auto& native = binding.native();
        EXPECT_EQ(native.data_.get(), snapshot.storage().ref().data);
        EXPECT_EQ(&native.use_state(), snapshot.resource_use().state.get());
        auto mapped = native.data();
        const std::uint32_t value = 91;
        mapped.write(std::as_bytes(std::span(&value, 1)));
        std::uint32_t result = 0;
        buffer.read_into(std::as_writable_bytes(std::span(&result, 1)));
        EXPECT_EQ(result, value);
    }
} // namespace
#endif
