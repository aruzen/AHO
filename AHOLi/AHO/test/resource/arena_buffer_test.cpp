#include <AHO/resource/arena_buffer.hpp>
#include <AHO/resource/typed_buffer.hpp>
#include <AHO/command/buffer_upload.hpp>
#include <VSL/lifetime/buffer_arena.hpp>
#include <array>

namespace {
    static_assert(aho::resource::gpu_buffer<aho::resource::ArenaBuffer>);
    static_assert(std::is_copy_constructible_v<aho::resource::ArenaBuffer>);
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
    TEST_F(ArenaBufferVulkanIntegrationTest, GenerationRetainsFactoryContextUntilItsFinalSnapshotIsDestroyed) {
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
        EXPECT_FALSE(context.expired());
        old.reset();
        EXPECT_TRUE(context.expired());
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
