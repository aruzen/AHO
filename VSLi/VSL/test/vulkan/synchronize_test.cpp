#include "support/vulkan_fixture.hpp"
namespace {
    class SynchronizeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        std::optional<VSL_NAMESPACE::SynchroManager> manager;
        void SetUp() override {
            VulkanIntegrationTest::SetUp();
            if (!HasFatalFailure() && !IsSkipped())
                manager.emplace(device);
        }
        void TearDown() override;
    };
    TEST_F(SynchronizeVulkanIntegrationTest, NamedLookupAndSemaphoreCounts) {
        EXPECT_FALSE(manager->get_fence("missing"));
        EXPECT_FALSE(manager->get_semaphore("missing"));
        EXPECT_FALSE((*manager)["missing"]);
        EXPECT_FALSE(manager->destroy_fence("missing"));
        EXPECT_FALSE(manager->destroy_semaphore("missing"));
        auto one = manager->create_semaphore("one"), many = manager->create_semaphore("many", 2);
        EXPECT_EQ(one.data_->semaphores.size(), 1u);
        EXPECT_EQ(many.data_->semaphores.size(), 2u);
        ASSERT_TRUE(manager->get_semaphore("many"));
        EXPECT_EQ(manager->get_semaphore("many")->data_, many.data_);
        EXPECT_NE(many.data_->semaphores[0], many.data_->semaphores[1]);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceWaitResetAndQueueSignalHaveObservableStates) {
        auto single = manager->create_fence("single", true);
        auto many = manager->create_fence("many", std::size_t{2}, true);
        ASSERT_EQ(single.data_->fences.size(), 1u);
        ASSERT_EQ(many.data_->fences.size(), 2u);
        EXPECT_EQ(manager->get_fence("single")->data_, single.data_);
        EXPECT_EQ((*manager)["many"]->data_, many.data_);
        EXPECT_TRUE(single.wait(std::uint64_t{0}));
        EXPECT_TRUE(many.wait(1, std::uint64_t{0}));
        single.reset();
        many.reset(1);
        EXPECT_FALSE(single.wait(std::uint64_t{0}));
        EXPECT_FALSE(many.wait(1, std::uint64_t{0}));
        auto queue = device.data_->device.getQueue(queue_family, 0);
        queue.submit(vk::SubmitInfo{}, single.data_->fences[0]);
        queue.submit(vk::SubmitInfo{}, many.data_->fences[1]);
        EXPECT_TRUE(single.wait());
        EXPECT_TRUE(many.wait(std::uint64_t{UINT64_MAX}));
    }
} // namespace

namespace {
    TEST_F(SynchronizeVulkanIntegrationTest, DestroyIsIdempotentAndDoesNotEraseReplacement) {
        auto old = manager->create_fence("same", true);
        auto replacement = manager->create_fence("same", true);
        old.destroy();
        old.destroy();
        EXPECT_TRUE(old.data_->fences.empty());
        ASSERT_TRUE(manager->get_fence("same"));
        EXPECT_EQ(manager->get_fence("same")->data_, replacement.data_);
        EXPECT_TRUE(replacement.wait(std::uint64_t{0}));
        EXPECT_TRUE(manager->destroy_fence("same"));
        EXPECT_FALSE(manager->destroy_fence("same"));
        EXPECT_THROW(replacement.wait(std::uint64_t{0}), std::logic_error);
        auto semaphore = manager->create_semaphore("signal", 2);
        auto alias = semaphore;
        semaphore.destroy();
        alias.destroy();
        EXPECT_EQ(alias.size(), 0u);
        EXPECT_FALSE(manager->get_semaphore("signal"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, HoldersOutliveManagerWithoutOwningIt) {
        auto fence = manager->create_fence("survivor", true);
        auto semaphore = manager->create_semaphore("survivor");
        std::weak_ptr<VSL_NAMESPACE::impl::SynchroManagerImpl> weak = manager->data_;
        manager.reset();
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(fence.wait(std::uint64_t{0}));
        semaphore.destroy();
        fence.destroy();
    }
} // namespace

namespace {
    TEST_F(SynchronizeVulkanIntegrationTest, SemaphoreDestroyRemovesOnlyItsNameAndDisarmsAllAliases) {
        auto value = manager->create_semaphore("a", 2);
        auto alias = value;
        auto retained = manager->create_semaphore("b");
        value.destroy();
        EXPECT_TRUE(alias.data_->semaphores.empty());
        EXPECT_FALSE(manager->get_semaphore("a"));
        ASSERT_TRUE(manager->get_semaphore("b"));
        EXPECT_EQ(manager->get_semaphore("b")->data_, retained.data_);
        // 検証に失敗しても、後続の確認でハンドルを二重破棄しないようにする。
        alias.data_->semaphores.clear();
        alias.destroy();
        VSL_NAMESPACE::SemaphoreHolder{}.destroy();
        EXPECT_TRUE(manager->destroy_semaphore("b"));
        EXPECT_TRUE(retained.data_->semaphores.empty());
        retained.data_->semaphores.clear();
        EXPECT_FALSE(manager->destroy_semaphore("b"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceDestroyRemovesOnlyItsNameAndDisarmsAllAliases) {
        auto value = manager->create_fence("a", std::size_t{2}, true);
        auto alias = value;
        auto retained = manager->create_fence("b", true);
        value.destroy();
        EXPECT_TRUE(alias.data_->fences.empty());
        EXPECT_FALSE(manager->get_fence("a"));
        ASSERT_TRUE(manager->get_fence("b"));
        EXPECT_TRUE(retained.wait(std::uint64_t{0}));
        alias.data_->fences.clear();
        alias.destroy();
        VSL_NAMESPACE::FenceHolder{}.destroy();
        EXPECT_TRUE(manager->destroy_fence("b"));
        EXPECT_TRUE(retained.data_->fences.empty());
        retained.data_->fences.clear();
        EXPECT_FALSE(manager->destroy_fence("b"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, DestroyingReplacedHoldersKeepsTheCurrentNamedObjectsAlive) {
        auto old_semaphore = manager->create_semaphore("same");
        auto old_fence = manager->create_fence("same", true);
        auto current_semaphore = manager->create_semaphore("same");
        auto current_fence = manager->create_fence("same", true);
        old_semaphore.destroy();
        old_fence.destroy();
        EXPECT_TRUE(old_semaphore.data_->semaphores.empty());
        EXPECT_TRUE(old_fence.data_->fences.empty());
        ASSERT_TRUE(manager->get_semaphore("same"));
        ASSERT_TRUE(manager->get_fence("same"));
        EXPECT_EQ(manager->get_semaphore("same")->data_, current_semaphore.data_);
        EXPECT_EQ(manager->get_fence("same")->data_, current_fence.data_);
        EXPECT_TRUE(current_fence.wait(std::uint64_t{0}));
    }

    void SynchronizeVulkanIntegrationTest::TearDown() {
        if (manager) {
            device.data_->device.waitIdle();
            manager.reset();
        }
        VulkanIntegrationTest::TearDown();
    }
} // namespace
