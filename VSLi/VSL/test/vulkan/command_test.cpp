#include "support/vulkan_fixture.hpp"
#include <limits>

namespace {
    using CommandVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    class CommandRangeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST_F(CommandRangeVulkanIntegrationTest, OutOfRangeResetIsRejectedAndValidBuffersRemainUsable) {
        auto pool = make_pool();
        vsl::CommandBuffer buffers(pool, 2);
        const auto handles = buffers.data_->command_buffers;
        for (const auto index : {2u, std::numeric_limits<std::uint32_t>::max()})
            EXPECT_THROW(buffers.reset(index), std::out_of_range);
        EXPECT_EQ(buffers.data_->command_buffers, handles);
        EXPECT_EQ(buffers.get_current_buffer_idx(), 0u);
        EXPECT_NO_THROW(buffers.reset(0));
        EXPECT_NO_THROW(buffers.reset(1));
        EXPECT_NO_THROW(buffers.reset());
    }
    TEST_F(CommandRangeVulkanIntegrationTest, ExclusiveBufferCountCannotBeTruncatedToOne) {
        if constexpr (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
            GTEST_SKIP() << "size_t has no values above uint32_t";
        } else {
            auto manager = make_manager();
            const auto count = std::size_t{std::numeric_limits<std::uint32_t>::max()} + 2;
            EXPECT_THROW(manager.make_exclusive_buffer(count), std::length_error);
            auto valid = manager.make_exclusive_buffer(1);
            EXPECT_EQ(valid.get_size(), 1u);
            EXPECT_NO_THROW(valid.reset(0));
        }
    }
    TEST_F(CommandVulkanIntegrationTest, PoolAndBuffersShareExpectedOwnerAndCanReset) {
        auto pool = make_pool();
        vsl::CommandPool borrowed(pool.data_);
        EXPECT_EQ(borrowed.data_, pool.data_);
        vsl::CommandBuffer buffers(pool, 2);
        EXPECT_EQ(buffers.get_size(), 2u);
        EXPECT_EQ(buffers.get_current_buffer_idx(), 0u);
        EXPECT_EQ(buffers.data_->command_pool, pool.data_);
        vsl::CommandBuffer shared(buffers.data_);
        EXPECT_EQ(shared.data_, buffers.data_);
        buffers.reset(0);
        buffers.reset();
        EXPECT_EQ(shared.get_current_buffer_idx(), 0u);
    }
    TEST_F(CommandVulkanIntegrationTest, NextCyclesThroughEveryBufferAndSharedViewsObserveTheSameIndex) {
        auto pool = make_pool();
        vsl::CommandBuffer buffers(pool, 3);
        vsl::CommandBuffer shared(buffers.data_);
        for (const auto expected : {1u, 2u, 0u, 1u}) {
            buffers.next();
            EXPECT_EQ(buffers.get_current_buffer_idx(), expected);
            EXPECT_EQ(shared.get_current_buffer_idx(), expected);
            EXPECT_EQ(buffers.get_size(), 3u);
        }
    }
    TEST_F(CommandVulkanIntegrationTest, NextOnASingleBufferAndManagerViewsPreservesOwnership) {
        auto manager = make_manager();
        auto single = manager.make_exclusive_buffer();
        single.next();
        single.next();
        EXPECT_EQ(single.get_current_buffer_idx(), 0u);
        auto current = manager.get_buffer();
        current.next();
        EXPECT_EQ(manager.get_current_buffer_idx(), 1u);
        EXPECT_EQ(manager.get_buffer().data_, current.data_);
    }
    TEST_F(CommandVulkanIntegrationTest, ManagerAccessorsAndExclusiveBuffersPreserveOwnership) {
        auto manager = make_manager();
        vsl::CommandManager shared(manager.data_);
        EXPECT_EQ(shared.data_, manager.data_);
        EXPECT_EQ(manager.get_pool().data_, manager.data_->command_pool);
        EXPECT_EQ(manager.get_buffer().data_, manager.data_->command_buffer);
        EXPECT_EQ(manager.get_current_buffer_idx(), manager.get_buffer().get_current_buffer_idx());
        auto exclusive = manager.make_exclusive_buffer(2);
        EXPECT_EQ(exclusive.get_size(), 2u);
        EXPECT_NE(exclusive.data_, manager.get_buffer().data_);
        EXPECT_EQ(exclusive.data_->command_pool, manager.get_pool().data_);
        EXPECT_NE(exclusive.data_->command_buffers[0], manager.get_buffer().data_->command_buffers[0]);
    }
    struct CommandShape {
        void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager);
    };
    struct DelegateShape {
        void invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager);
    };
    static_assert(vsl::command::is_command<CommandShape>);
    static_assert(!vsl::command::is_command<DelegateShape>);
    static_assert(vsl::command::is_delegate_command<DelegateShape>);
    static_assert(!vsl::command::is_delegate_command<int>);
} // namespace

namespace {
    struct DestructionProbe : vsl::command::Command, vsl::command::DelegateCommand, vsl::command::PipelineHolder, vsl::command::PipelineRequire, vsl::command::VertexSizeHolder, vsl::command::VertexSizeRequire {
        int& destroyed;
        explicit DestructionProbe(int& destroyed) : destroyed(destroyed) {}
        ~DestructionProbe() { ++destroyed; }
        void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager) override {}
        void invoke(vsl::DefaultPhaseStream&, vsl::CommandPool, vsl::CommandBuffer,
                    vsl::CommandManager) override {}
        vsl::PipelineAccessor get_pipeline() override { return {}; }
        void set_pipeline(std::optional<vsl::PipelineAccessor>) override {}
        std::size_t get_vertex_size() override { return 0; }
        void set_vertex_size(std::optional<std::size_t>) override {}
    };
    template<class T>
    class CommandInterfaceLifetimeTest : public testing::Test {};
    using CommandInterfaces = testing::Types<vsl::command::Command, vsl::command::DelegateCommand,
                                             vsl::command::PipelineHolder, vsl::command::PipelineRequire,
                                             vsl::command::VertexSizeHolder, vsl::command::VertexSizeRequire>;
    TYPED_TEST_SUITE(CommandInterfaceLifetimeTest, CommandInterfaces);
    TYPED_TEST(CommandInterfaceLifetimeTest, UniqueOwnershipThroughTheBaseDestroysTheCompleteCommandOnce) {
        static_assert(std::has_virtual_destructor_v<TypeParam>);
        static_assert(std::is_copy_constructible_v<DestructionProbe>);
        static_assert(std::is_move_constructible_v<DestructionProbe>);
        int destroyed = 0;
        {
            std::unique_ptr<TypeParam> base = std::make_unique<DestructionProbe>(destroyed);
            EXPECT_EQ(destroyed, 0);
        }
        EXPECT_EQ(destroyed, 1);
    }
} // namespace

namespace {
    using CommandResourceVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    TEST_F(CommandResourceVulkanIntegrationTest, VertexBindingAddsAnOwnerToTheRecordingSubmission) {
        auto manager = make_manager();
        vsl::StagingBuffer<vsl::MemoryType::VERTEX_BUFFER> vertex(device, 32);
        const auto owners = vertex.data_.use_count();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::BindVertexBuffer(vertex);
        EXPECT_GT(vertex.data_.use_count(), owners);
        phase.cancel();
    }
    TEST_F(CommandResourceVulkanIntegrationTest, IndexBindingAddsAnOwnerToTheRecordingSubmission) {
        auto manager = make_manager();
        vsl::StagingBuffer<vsl::MemoryType::INDEX_BUFFER> indices(device, 12);
        const auto owners = indices.data_.use_count();
        auto phase = manager.start_phase<vsl::ComputePhase>();
        phase << vsl::command::BindIndexBuffer(indices);
        EXPECT_GT(indices.data_.use_count(), owners);
        phase.cancel();
    }
} // namespace

namespace {
    TEST_F(CommandResourceVulkanIntegrationTest, CancellationReleasesRetainedBuffersWhenTheTokenIsDropped) {
        auto manager = make_manager();
        std::weak_ptr<vsl::impl::BufferImpl> weak;
        vsl::Completion cancelled;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            cancelled = phase.completion();
            {
                vsl::StagingBuffer<vsl::MemoryType::VERTEX_BUFFER> vertex(device, 32);
                weak = vertex.data_;
                phase << vsl::command::BindVertexBuffer(vertex);
            }
            EXPECT_FALSE(weak.expired());
            phase.cancel();
        }
        EXPECT_EQ(cancelled.status(), vsl::Completion::Status::CANCELLED);
        EXPECT_FALSE(weak.expired());
        cancelled = {};
        EXPECT_TRUE(weak.expired());
    }
    TEST_F(CommandResourceVulkanIntegrationTest, CustomCommandsCanRetainOwnersUntilFrameSlotReuse) {
        auto manager = make_manager();
        std::weak_ptr<int> weak;
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto owner = std::make_shared<int>(42);
            weak = owner;
            manager.get_buffer().retain(owner);
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_FALSE(weak.expired());
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            ASSERT_TRUE(phase.completion().wait());
        }
        EXPECT_TRUE(weak.expired());
    }
} // namespace
