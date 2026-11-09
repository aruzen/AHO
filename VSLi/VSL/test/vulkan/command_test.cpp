#include <VSL/lifetime/command_access.hpp>
#include "support/vulkan_fixture.hpp"
#include <VSL/lifetime/command_domain.hpp>
#include <VSL/lifetime/command_storage.hpp>
#include "support/deferred_action.hpp"
#include <limits>

namespace {
    using VSL_NAMESPACE::lifetime::CommandAccess;
}
namespace {
    using CommandVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
    class CommandRangeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };
    TEST(CommandFacadeTest, EmptyValuesHaveNoIdentityAndRejectResourceOperations) {
        vsl::CommandPool pool;
        vsl::CommandBuffer buffer;
        vsl::CommandManager manager;
        EXPECT_FALSE(pool);
        EXPECT_FALSE(buffer);
        EXPECT_FALSE(manager);
        EXPECT_EQ(pool.identity(), vsl::CommandPoolIdentity{});
        EXPECT_EQ(buffer.identity(), vsl::CommandBufferIdentity{});
        EXPECT_EQ(manager.identity(), vsl::CommandManagerIdentity{});
        EXPECT_FALSE(buffer.completion());
        EXPECT_THROW(pool.get_device(), std::invalid_argument);
        EXPECT_THROW(buffer.get_pool(), std::invalid_argument);
        EXPECT_THROW(buffer.get_size(), std::invalid_argument);
        EXPECT_THROW(buffer.get_current_buffer_idx(), std::invalid_argument);
        EXPECT_THROW(buffer.require_recording(), std::logic_error);
        EXPECT_THROW(manager.get_device(), std::invalid_argument);
        EXPECT_THROW(manager.get_pool(), std::invalid_argument);
        EXPECT_THROW(manager.get_buffer(), std::invalid_argument);
        EXPECT_THROW(manager.get_current_buffer_idx(), std::invalid_argument);
        EXPECT_THROW(manager.make_exclusive_buffer(), std::invalid_argument);
    }
    TEST_F(CommandRangeVulkanIntegrationTest, ManagerIdentityIsSeparateFromTheSharedRingAndPool) {
        auto pool = make_pool();
        vsl::CommandBuffer buffer(pool, 1);
        auto manager = vsl::CommandManager::FromBuffer(buffer);
        const auto alias = manager;
        auto other = vsl::CommandManager::FromBuffer(buffer);
        EXPECT_EQ(manager.identity(), alias.identity());
        EXPECT_NE(manager.identity(), other.identity());
        EXPECT_EQ(manager.get_buffer().identity(), other.get_buffer().identity());
        EXPECT_EQ(manager.get_pool().identity(), pool.identity());
        EXPECT_EQ(other.get_pool().identity(), pool.identity());
        const auto identity = manager.identity();
        auto moved = std::move(manager);
        EXPECT_FALSE(manager);
        EXPECT_EQ(moved.identity(), identity);
        EXPECT_THROW(manager.get_pool(), std::invalid_argument);
        EXPECT_THROW(manager.get_current_buffer_idx(), std::invalid_argument);
    }
    TEST_F(CommandRangeVulkanIntegrationTest, OutOfRangeResetIsRejectedAndValidBuffersRemainUsable) {
        auto pool = make_pool();
        vsl::CommandBuffer buffers(pool, 2);
        const auto handles = CommandAccess::Data(buffers)->command_buffers;
        for (const auto index : {2u, std::numeric_limits<std::uint32_t>::max()})
            EXPECT_THROW(buffers.reset(index), std::out_of_range);
        EXPECT_EQ(CommandAccess::Data(buffers)->command_buffers, handles);
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
    TEST_F(CommandVulkanIntegrationTest, DefaultBufferCountAgreesAcrossTheCallerAndLibrary) {
        struct RestoreCount {
            std::uint32_t previous = vsl::DEFAULT_BUFFER_SIZE;
            ~RestoreCount() { vsl::DEFAULT_BUFFER_SIZE = previous; }
        } restore;
        auto pool = make_pool();
        for (const auto count : {1u, 2u}) {
            vsl::DEFAULT_BUFFER_SIZE = count;
            vsl::CommandBuffer direct(pool);
            auto manager = make_manager();
            EXPECT_EQ(direct.get_size(), count);
            EXPECT_EQ(manager.get_buffer().get_size(), count);
            vsl::CommandBuffer explicit_count(pool, 4);
            EXPECT_EQ(explicit_count.get_size(), 4u);
        }
    }
    TEST_F(CommandVulkanIntegrationTest, PoolAndBuffersShareExpectedOwnerAndCanReset) {
        auto pool = make_pool();
        vsl::CommandPool borrowed(CommandAccess::Pool(CommandAccess::Data(pool)));
        EXPECT_EQ(CommandAccess::Data(borrowed), CommandAccess::Data(pool));
        vsl::CommandBuffer buffers(pool, 2);
        EXPECT_EQ(buffers.get_size(), 2u);
        EXPECT_EQ(buffers.get_current_buffer_idx(), 0u);
        EXPECT_EQ(CommandAccess::Data(buffers)->command_pool, CommandAccess::Data(pool).get());
        vsl::CommandBuffer shared(CommandAccess::RawBuffer(CommandAccess::Data(buffers)));
        EXPECT_EQ(CommandAccess::Data(shared), CommandAccess::Data(buffers));
        buffers.reset(0);
        buffers.reset();
        EXPECT_EQ(shared.get_current_buffer_idx(), 0u);
    }
    TEST_F(CommandVulkanIntegrationTest, NextCyclesThroughEveryBufferAndSharedViewsObserveTheSameIndex) {
        auto pool = make_pool();
        vsl::CommandBuffer buffers(pool, 3);
        vsl::CommandBuffer shared(CommandAccess::RawBuffer(CommandAccess::Data(buffers)));
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
        EXPECT_EQ(CommandAccess::Data(manager.get_buffer()), CommandAccess::Data(current));
    }
    TEST_F(CommandVulkanIntegrationTest, ManagerAccessorsAndExclusiveBuffersPreserveOwnership) {
        auto manager = make_manager();
        vsl::CommandManager shared(CommandAccess::RawManager(CommandAccess::Data(manager)));
        EXPECT_EQ(CommandAccess::Data(shared), CommandAccess::Data(manager));
        EXPECT_EQ(CommandAccess::Data(manager.get_pool()), CommandAccess::Data(manager)->command_pool);
        EXPECT_EQ(CommandAccess::Data(manager.get_buffer()), CommandAccess::Data(manager)->command_buffer);
        EXPECT_EQ(manager.get_current_buffer_idx(), manager.get_buffer().get_current_buffer_idx());
        auto exclusive = manager.make_exclusive_buffer(2);
        EXPECT_EQ(exclusive.get_size(), 2u);
        EXPECT_NE(CommandAccess::Data(exclusive), CommandAccess::Data(manager.get_buffer()));
        EXPECT_EQ(CommandAccess::Data(exclusive)->command_pool, CommandAccess::Data(manager.get_pool()).get());
        EXPECT_NE(CommandAccess::Data(exclusive)->command_buffers[0], CommandAccess::Data(manager.get_buffer())->command_buffers[0]);
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
    class CommandAllocationVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        bool validate_vulkan() const override;
    };

    bool CommandAllocationVulkanIntegrationTest::validate_vulkan() const {
        return true;
    }

    TEST_F(CommandAllocationVulkanIntegrationTest, RejectsZeroBufferCountsBeforeCallingVulkan) {
        // 検証エラーはテストを失敗させ、不正な割当・解放をdriverへ渡さない。
        expect_validation_abort("");
        auto pool = make_pool();
        EXPECT_THROW((vsl::CommandBuffer(pool, 0)), std::invalid_argument);
        struct RestoreCount {
            std::uint32_t previous = vsl::DEFAULT_BUFFER_SIZE;
            ~RestoreCount() { vsl::DEFAULT_BUFFER_SIZE = previous; }
        } restore;
        vsl::DEFAULT_BUFFER_SIZE = 0;
        EXPECT_THROW((vsl::CommandBuffer(pool)), std::invalid_argument);
    }

    TEST_F(CommandAllocationVulkanIntegrationTest, UnallocatedStorageCanUnwindWithoutFreeingNativeBuffers) {
        expect_validation_abort("");
        auto pool = make_pool();
        {
            // 割当が失敗したconstructorの後処理と同じ、poolだけを保持した状態。
            auto storage = std::make_shared<vsl::impl::CommandBufferImpl>();
            storage->command_pool = CommandAccess::Data(pool).get();
        }
    }
} // namespace

namespace {
    TEST_F(CommandResourceVulkanIntegrationTest, CancelledResultDoesNotKeepBuffersAliveAfterThePhaseIsDestroyed) {
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
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(cancelled.data_->work.expired());
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

    TEST_F(CommandResourceVulkanIntegrationTest, NativeStorageDoesNotOwnTheWorkThatRetainsIt) {
        std::weak_ptr<vsl::impl::CommandBufferImpl> native;
        std::weak_ptr<vsl::lifetime::CommandDomain> domain;
        std::weak_ptr<int> retained;
        vsl::Completion result;
        {
            auto manager = make_manager().independent(1);
            auto buffer = manager.get_buffer();
            native = CommandAccess::Data(buffer);
            domain = vsl::lifetime::CommandAccess::Owner(buffer);
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto owner = std::make_shared<int>(42);
            retained = owner;
            buffer.retain(owner);
            buffer.retain(CommandAccess::Data(buffer));
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
            result = phase.completion();
        }
        EXPECT_TRUE(domain.expired());
        EXPECT_TRUE(native.expired());
        EXPECT_TRUE(retained.expired());
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
        EXPECT_TRUE(result.data_->work.expired());
    }

    TEST_F(CommandResourceVulkanIntegrationTest, BufferFacadeKeepsCanonicalDomainAfterManagerDestruction) {
        vsl::CommandBuffer saved(CommandAccess::RawBuffer(nullptr));
        std::weak_ptr<vsl::lifetime::CommandDomain> domain;
        std::weak_ptr<int> retained;
        {
            auto manager = make_manager().independent(1);
            saved = manager.get_buffer();
            const auto shared = vsl::CommandBuffer(CommandAccess::RawBuffer(CommandAccess::Data(saved)));
            domain = vsl::lifetime::CommandAccess::Owner(saved);
            EXPECT_EQ(domain.lock(), vsl::lifetime::CommandAccess::Owner(shared));
            auto phase = manager.start_phase<vsl::ComputePhase>();
            auto owner = std::make_shared<int>(9);
            retained = owner;
            saved.retain(owner);
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_FALSE(domain.expired());
        EXPECT_FALSE(retained.expired());
        saved = vsl::CommandBuffer(CommandAccess::RawBuffer(nullptr));
        EXPECT_TRUE(domain.expired());
        EXPECT_TRUE(retained.expired());
    }

    TEST_F(CommandResourceVulkanIntegrationTest, ExpiredNativeOnlyAdaptersCannotRecreateADomain) {
        std::shared_ptr<vsl::impl::CommandBufferImpl> native;
        std::shared_ptr<vsl::impl::CommandManagerImpl> manager_data;
        {
            auto manager = make_manager().independent(1);
            manager_data = CommandAccess::Data(manager);
            native = CommandAccess::Data(manager.get_buffer());
        }
        ASSERT_TRUE(native);
        EXPECT_TRUE(native->owner_context->domain.expired());
        EXPECT_EQ(native->recording_context, nullptr);
        EXPECT_THROW(vsl::CommandBuffer{CommandAccess::RawBuffer(native)}, std::logic_error);
        EXPECT_THROW(vsl::CommandManager{CommandAccess::RawManager(manager_data)}, std::logic_error);
    }

    TEST_F(CommandResourceVulkanIntegrationTest, PhaseOwnsDomainWithoutTheOriginalManagerFacade) {
        std::optional<vsl::CommandManager> manager(make_manager().independent(1));
        std::weak_ptr<vsl::lifetime::CommandDomain> domain = vsl::lifetime::CommandAccess::Owner(manager->get_buffer());
        vsl::Completion result;
        {
            auto phase = manager->start_phase<vsl::ComputePhase>();
            result = phase.completion();
            manager.reset();
            EXPECT_FALSE(domain.expired());
            EXPECT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_TRUE(domain.expired());
        EXPECT_EQ(result.status(), vsl::Completion::Status::COMPLETE);
    }

    TEST_F(CommandResourceVulkanIntegrationTest, ManagerAssignmentPublishesBothOwnersBeforeOldCallbacks) {
        auto manager = make_manager().independent(1);
        auto replacement = make_manager().independent(1);
        bool coherent = false;
        bool rejected_reconnection = false;
        const auto old_native = CommandAccess::Data(manager.get_buffer());
        std::weak_ptr<vsl::lifetime::CommandDomain> old_domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        const vsl_test::DeferredAction callback([&] {
            coherent = CommandAccess::Data(manager) == CommandAccess::Data(replacement) &&
                       vsl::lifetime::CommandAccess::Owner(manager.get_buffer())->buffer == CommandAccess::Data(manager)->command_buffer;
            try {
                vsl::CommandBuffer reconnect(CommandAccess::RawBuffer(old_native));
            } catch (const std::logic_error&) {
                rejected_reconnection = true;
            }
            auto next = manager.start_phase<vsl::ComputePhase>();
            next.cancel();
        });
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            manager.get_buffer().retain(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        manager = replacement;
        EXPECT_TRUE(coherent);
        EXPECT_TRUE(rejected_reconnection);
        EXPECT_TRUE(old_domain.expired());
    }

    TEST_F(CommandResourceVulkanIntegrationTest, WaitIdlePreservesANewSubmissionCreatedByACompletionCallback) {
        auto manager = make_manager().independent(1);
        vsl::Completion next_result;
        std::weak_ptr<int> next_owner;
        bool invoked = false;
        const vsl_test::DeferredAction callback([&] {
            invoked = true;
            auto next = manager.start_phase<vsl::ComputePhase>();
            auto resource = std::make_shared<int>(73);
            next_owner = resource;
            manager.get_buffer().retain(resource);
            next_result = next.completion();
            EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        });
        {
            auto phase = manager.start_phase<vsl::ComputePhase>();
            manager.get_buffer().retain_until_complete(callback.owner());
            ASSERT_EQ(phase.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        manager.wait_idle();
        EXPECT_TRUE(invoked);
        ASSERT_TRUE(next_result);
        EXPECT_FALSE(next_owner.expired());
        const auto domain = vsl::lifetime::CommandAccess::Owner(manager.get_buffer());
        ASSERT_TRUE(domain->submissions.at(0));
        EXPECT_EQ(domain->submissions.at(0)->result, next_result.data_);
        EXPECT_TRUE(next_result.wait());
        {
            auto next = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
        EXPECT_TRUE(next_owner.expired());
    }

    TEST_F(CommandResourceVulkanIntegrationTest, WaitIdleLeavesRecordingPending) {
        auto manager = make_manager().independent(1);
        auto next = manager.start_phase<vsl::ComputePhase>();
        manager.wait_idle();
        EXPECT_EQ(next.completion().status(), vsl::Completion::Status::PENDING);
        EXPECT_EQ(manager.get_buffer().completion().data_, next.completion().data_);
        EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
    }

    TEST_F(CommandResourceVulkanIntegrationTest, CancelAndFailureCallbacksCanDestroyTheirPhase) {
        for (const bool fail : {false, true}) {
            auto manager = make_manager().independent(1);
            vsl::SynchroManager sync(device);
            const auto available = sync.create_semaphore("unused wait");
            auto phase = std::make_unique<vsl::ComputePhase>(manager, fail ? std::optional(available) : std::nullopt,
                                                             std::nullopt, std::nullopt, false, static_cast<vsl::PipelineStage>(0));
            const auto result = phase->completion();
            int releases = 0;
            const vsl_test::DeferredAction callback([&] {
                ++releases;
                phase.reset();
            });
            manager.get_buffer().retain_until_complete(callback.owner());
            if (fail)
                EXPECT_THROW(phase->submit(), std::invalid_argument);
            else
                phase->cancel();
            EXPECT_FALSE(phase);
            EXPECT_EQ(releases, 1);
            EXPECT_EQ(result.status(), fail ? vsl::Completion::Status::FAILED : vsl::Completion::Status::CANCELLED);
            EXPECT_TRUE(result.data_->work.expired());
            auto next = manager.start_phase<vsl::ComputePhase>();
            EXPECT_EQ(next.submit(), vsl::ComputePhase::SubmitResult::SUCCESS);
        }
    }
} // namespace
