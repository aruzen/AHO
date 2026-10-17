#include "support/vulkan_fixture.hpp"

namespace {
using CommandVulkanIntegrationTest = vsl_test::VulkanIntegrationTest;
TEST_F(CommandVulkanIntegrationTest, PoolAndBuffersShareExpectedOwnerAndCanReset) {
    auto pool = make_pool();
    vsl::CommandPool borrowed(pool._data);
    EXPECT_EQ(borrowed._data, pool._data);
    vsl::CommandBuffer buffers(pool, 2);
    EXPECT_EQ(buffers.getSize(), 2u);
    EXPECT_EQ(buffers.getCurrentBufferIdx(), 0u);
    EXPECT_EQ(buffers._data->commandPool, pool._data);
    vsl::CommandBuffer shared(buffers._data);
    EXPECT_EQ(shared._data, buffers._data);
    buffers.reset(0);
    buffers.reset();
    EXPECT_EQ(shared.getCurrentBufferIdx(), 0u);
}
TEST_F(CommandVulkanIntegrationTest, NextCyclesThroughEveryBufferAndSharedViewsObserveTheSameIndex) {
    auto pool = make_pool();
    vsl::CommandBuffer buffers(pool, 3);
    vsl::CommandBuffer shared(buffers._data);
    for (const auto expected : {1u, 2u, 0u, 1u}) {
        buffers.next();
        EXPECT_EQ(buffers.getCurrentBufferIdx(), expected);
        EXPECT_EQ(shared.getCurrentBufferIdx(), expected);
        EXPECT_EQ(buffers.getSize(), 3u);
    }
}
TEST_F(CommandVulkanIntegrationTest, NextOnASingleBufferAndManagerViewsPreservesOwnership) {
    auto manager = make_manager();
    auto single = manager.makeExclusiveBuffer();
    single.next();
    single.next();
    EXPECT_EQ(single.getCurrentBufferIdx(), 0u);
    auto current = manager.getBuffer();
    current.next();
    EXPECT_EQ(manager.getCurrentBufferIdx(), 1u);
    EXPECT_EQ(manager.getBuffer()._data, current._data);
}
TEST_F(CommandVulkanIntegrationTest, ManagerAccessorsAndExclusiveBuffersPreserveOwnership) {
    auto manager = make_manager();
    vsl::CommandManager shared(manager._data);
    EXPECT_EQ(shared._data, manager._data);
    EXPECT_EQ(manager.getPool()._data, manager._data->commandPool);
    EXPECT_EQ(manager.getBuffer()._data, manager._data->commandBuffer);
    EXPECT_EQ(manager.getCurrentBufferIdx(), manager.getBuffer().getCurrentBufferIdx());
    auto exclusive = manager.makeExclusiveBuffer(2);
    EXPECT_EQ(exclusive.getSize(), 2u);
    EXPECT_NE(exclusive._data, manager.getBuffer()._data);
    EXPECT_EQ(exclusive._data->commandPool, manager.getPool()._data);
    EXPECT_NE(exclusive._data->commandBuffers[0], manager.getBuffer()._data->commandBuffers[0]);
}
struct CommandShape {
    void invoke(vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager);
};
struct DelegateShape {
    void invoke(vsl::DefaultPhaseStream &, vsl::CommandPool, vsl::CommandBuffer, vsl::CommandManager);
};
static_assert(vsl::command::is_command<CommandShape>);
static_assert(!vsl::command::is_command<DelegateShape>);
static_assert(vsl::command::is_delegate_command<DelegateShape>);
static_assert(!vsl::command::is_delegate_command<int>);
} // namespace
