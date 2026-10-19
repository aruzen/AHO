#include "support/vulkan_fixture.hpp"
namespace {
    class SynchronizeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
    protected:
        bool validate_vulkan() const override { return true; }
        std::optional<vsl::SynchroManager> manager;
        void SetUp() override {
            VulkanIntegrationTest::SetUp();
            if (!HasFatalFailure() && !IsSkipped())
                manager.emplace(device);
        }
        void TearDown() override {
            if (manager) {
                device._data->device.waitIdle();
                manager.reset();
            }
            VulkanIntegrationTest::TearDown();
        }
    };
    TEST_F(SynchronizeVulkanIntegrationTest, NamedLookupAndSemaphoreCounts) {
        EXPECT_FALSE(manager->getFence("missing"));
        EXPECT_FALSE(manager->getSemaphore("missing"));
        EXPECT_FALSE((*manager)["missing"]);
        EXPECT_FALSE(manager->destroyFence("missing"));
        EXPECT_FALSE(manager->destroySemaphore("missing"));
        auto one = manager->createSemaphore("one"), many = manager->createSemaphore("many", 2);
        EXPECT_EQ(one._data->semaphores.size(), 1u);
        EXPECT_EQ(many._data->semaphores.size(), 2u);
        ASSERT_TRUE(manager->getSemaphore("many"));
        EXPECT_EQ(manager->getSemaphore("many")->_data, many._data);
        EXPECT_NE(many._data->semaphores[0], many._data->semaphores[1]);
    }
    TEST_F(SynchronizeVulkanIntegrationTest, FenceWaitResetAndQueueSignalHaveObservableStates) {
        auto single = manager->createFence("single", true);
        auto many = manager->createFence("many", std::size_t{2}, true);
        ASSERT_EQ(single._data->fences.size(), 1u);
        ASSERT_EQ(many._data->fences.size(), 2u);
        EXPECT_EQ(manager->getFence("single")->_data, single._data);
        EXPECT_EQ((*manager)["many"]->_data, many._data);
        EXPECT_TRUE(single.wait(std::uint64_t{0}));
        EXPECT_TRUE(many.wait(1, std::uint64_t{0}));
        single.reset();
        many.reset(1);
        EXPECT_FALSE(single.wait(std::uint64_t{0}));
        EXPECT_FALSE(many.wait(1, std::uint64_t{0}));
        auto queue = device._data->device.getQueue(queue_family, 0);
        queue.submit(vk::SubmitInfo{}, single._data->fences[0]);
        queue.submit(vk::SubmitInfo{}, many._data->fences[1]);
        EXPECT_TRUE(single.wait());
        EXPECT_TRUE(many.wait(std::uint64_t{UINT64_MAX}));
    }
}

namespace {
    TEST_F(SynchronizeVulkanIntegrationTest, DestroyIsIdempotentAndDoesNotEraseReplacement) {
        auto old = manager->createFence("same", true);
        auto replacement = manager->createFence("same", true);
        old.destroy();
        old.destroy();
        EXPECT_TRUE(old._data->fences.empty());
        ASSERT_TRUE(manager->getFence("same"));
        EXPECT_EQ(manager->getFence("same")->_data, replacement._data);
        EXPECT_TRUE(replacement.wait(std::uint64_t{0}));
        EXPECT_TRUE(manager->destroyFence("same"));
        EXPECT_FALSE(manager->destroyFence("same"));
        EXPECT_THROW(replacement.wait(std::uint64_t{0}), std::logic_error);
        auto semaphore = manager->createSemaphore("signal", 2);
        auto alias = semaphore;
        semaphore.destroy();
        alias.destroy();
        EXPECT_EQ(alias.size(), 0u);
        EXPECT_FALSE(manager->getSemaphore("signal"));
    }
    TEST_F(SynchronizeVulkanIntegrationTest, HoldersOutliveManagerWithoutOwningIt) {
        auto fence = manager->createFence("survivor", true);
        auto semaphore = manager->createSemaphore("survivor");
        std::weak_ptr<vsl::_impl::SynchroManager_impl> weak = manager->_data;
        manager.reset();
        EXPECT_TRUE(weak.expired());
        EXPECT_TRUE(fence.wait(std::uint64_t{0}));
        semaphore.destroy();
        fence.destroy();
    }
}

namespace {
TEST_F(SynchronizeVulkanIntegrationTest, SemaphoreDestroyRemovesOnlyItsNameAndDisarmsAllAliases) {
    auto value = manager->createSemaphore("a", 2);
    auto alias = value;
    auto retained = manager->createSemaphore("b");
    value.destroy();
    EXPECT_TRUE(alias._data->semaphores.empty());
    EXPECT_FALSE(manager->getSemaphore("a"));
    ASSERT_TRUE(manager->getSemaphore("b"));
    EXPECT_EQ(manager->getSemaphore("b")->_data, retained._data);
    // 検証に失敗しても、後続の確認でハンドルを二重破棄しないようにする。
    alias._data->semaphores.clear();
    alias.destroy();
    vsl::SemaphoreHolder{}.destroy();
    EXPECT_TRUE(manager->destroySemaphore("b"));
    EXPECT_TRUE(retained._data->semaphores.empty());
    retained._data->semaphores.clear();
    EXPECT_FALSE(manager->destroySemaphore("b"));
}
TEST_F(SynchronizeVulkanIntegrationTest, FenceDestroyRemovesOnlyItsNameAndDisarmsAllAliases) {
    auto value = manager->createFence("a", std::size_t{2}, true);
    auto alias = value;
    auto retained = manager->createFence("b", true);
    value.destroy();
    EXPECT_TRUE(alias._data->fences.empty());
    EXPECT_FALSE(manager->getFence("a"));
    ASSERT_TRUE(manager->getFence("b"));
    EXPECT_TRUE(retained.wait(std::uint64_t{0}));
    alias._data->fences.clear();
    alias.destroy();
    vsl::FenceHolder{}.destroy();
    EXPECT_TRUE(manager->destroyFence("b"));
    EXPECT_TRUE(retained._data->fences.empty());
    retained._data->fences.clear();
    EXPECT_FALSE(manager->destroyFence("b"));
}
TEST_F(SynchronizeVulkanIntegrationTest, DestroyingReplacedHoldersKeepsTheCurrentNamedObjectsAlive) {
    auto old_semaphore = manager->createSemaphore("same");
    auto old_fence = manager->createFence("same", true);
    auto current_semaphore = manager->createSemaphore("same");
    auto current_fence = manager->createFence("same", true);
    old_semaphore.destroy();
    old_fence.destroy();
    EXPECT_TRUE(old_semaphore._data->semaphores.empty());
    EXPECT_TRUE(old_fence._data->fences.empty());
    ASSERT_TRUE(manager->getSemaphore("same"));
    ASSERT_TRUE(manager->getFence("same"));
    EXPECT_EQ(manager->getSemaphore("same")->_data, current_semaphore._data);
    EXPECT_EQ(manager->getFence("same")->_data, current_fence._data);
    EXPECT_TRUE(current_fence.wait(std::uint64_t{0}));
}
}
