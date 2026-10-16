#include "support/vulkan_fixture.hpp"
namespace {
class SynchronizeVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
  protected:
    std::optional<vsl::SynchroManager> manager;
    void SetUp() override {
        VulkanIntegrationTest::SetUp();
        if (!HasFatalFailure() && !IsSkipped())
            manager.emplace(device);
    }
    void TearDown() override {
        // Holder::destroy erases while iterating (I23). Reclaim fixture-owned handles safely.
        if (manager) {
            device._data->device.waitIdle();
            for (auto &[name, holder] : manager->_data->fences)
                for (auto fence : holder->fences)
                    device._data->device.destroyFence(fence);
            for (auto &[name, holder] : manager->_data->semaphores)
                for (auto semaphore : holder->semaphores)
                    device._data->device.destroySemaphore(semaphore);
            manager->_data->fences.clear();
            manager->_data->semaphores.clear();
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
} // namespace
