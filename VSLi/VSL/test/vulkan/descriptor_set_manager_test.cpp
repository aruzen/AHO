#include "support/compute_fixture.hpp"
namespace {
using DescriptorManagerVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
TEST_F(DescriptorManagerVulkanIntegrationTest, MakeAndExactAllocateReturnUsableResources) {
    auto [allocated_pool, allocated] = resources->make(std::vector{*binding});
    ASSERT_EQ(allocated.size(), 1u);
    EXPECT_EQ(allocated_pool.getManager(), &*resources);
    allocated.front().update(&*output);
    resource = allocated.front();
    expect_sequence(execute(41), 41);
    allocated_pool._data->resources.clear();
    auto [pools, exact] = resources->allocate(std::vector{*binding, *binding});
    ASSERT_EQ(pools.size(), 2u);
    ASSERT_EQ(exact.size(), 2u);
    EXPECT_EQ(exact[0].getPool()._data, pools[0]._data);
    EXPECT_EQ(exact[1].getPool()._data, pools[1]._data);
    for (auto &entry : pools)
        entry._data->resources.clear();
}
TEST_F(DescriptorManagerVulkanIntegrationTest, DefaultOverAllocatorStartsWithoutPools) {
    vsl::graphic_resource::OverAllocateManager manager(device);
    EXPECT_EQ(manager._data->device, device._data);
    EXPECT_TRUE(manager.pools.empty());
}
TEST_F(DescriptorManagerVulkanIntegrationTest,
       DISABLED_OverAllocateReturnsOnePoolPerResourceAndReusesCapacity) {
    using namespace vsl::graphic_resource;
    OverAllocateManager manager(device, {{Type::StorageBuffer, 4}}, 0);
    auto [pools, allocated] = manager.allocate(std::vector{*binding});
    EXPECT_EQ(pools.size(), allocated.size()); // I12: result pool vector remains empty.
    ASSERT_EQ(allocated.size(), 1u);
    allocated.front().update(&*output);
    for (auto &entry : manager._data->pools)
        entry->resources.clear();
}
} // namespace
namespace {
TEST_F(DescriptorManagerVulkanIntegrationTest, TemplateMakeAndAllocateOverloadsUpdateTheirBuffers) {
    auto &manager = static_cast<vsl::graphic_resource::ManagerInterface &>(*resources);
    auto use = [&](auto allocated, std::uint32_t base) {
        ASSERT_EQ(allocated.size(), 1u);
        resource = allocated.front();
        expect_sequence(execute(base), base);
    };
    auto [make_pool, make_resources] = manager.make(std::vector{*binding}, std::vector{*output});
    use(make_resources, 59);
    auto [bound_pool, bound_resources] =
        manager.make(std::vector{*binding}, std::vector{*output}, std::vector<std::size_t>{0});
    use(bound_resources, 61);
    auto [allocate_pools, allocated] = manager.allocate(std::vector{*binding}, std::vector{*output});
    EXPECT_EQ(allocate_pools.size(), 1u);
    use(allocated, 67);
    auto [explicit_pools, explicit_resources] =
        manager.allocate(std::vector{*binding}, std::vector{*output}, std::vector<std::size_t>{0});
    EXPECT_EQ(explicit_pools.size(), 1u);
    use(explicit_resources, 71);
}
TEST_F(DescriptorManagerVulkanIntegrationTest, DISABLED_OverAllocatorDestroyReleasesItsOwnedPool) {
    using namespace vsl::graphic_resource;
    OverAllocateManager manager(device, {{Type::StorageBuffer, 1}}, 0);
    ASSERT_EQ(manager.pools.size(), 1u);
    auto allocated_pool = manager.pools.front();
    manager.destroy(allocated_pool);
    EXPECT_TRUE(manager.pools.empty());
    EXPECT_TRUE(manager._data->pools.empty());
}
} // namespace

#include <type_traits>
namespace {
using ResourceManagerInterface = vsl::graphic_resource::ManagerInterface;
static_assert(std::has_virtual_destructor_v<ResourceManagerInterface>);
struct StubResourceManager : ResourceManagerInterface {
    std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>>
    allocate(std::vector<vsl::graphic_resource::BindingLayout>) override { return {}; }
    void destroy(const vsl::graphic_resource::Pool &) override {}
};
TEST(ResourceManagerLifetimeTest, BaseOwnerRunsTheDerivedDestructor) {
    struct TrackedManager : StubResourceManager {
        bool &destroyed;
        explicit TrackedManager(bool &destroyed) : destroyed(destroyed) {}
        ~TrackedManager() { destroyed = true; }
    };
    bool destroyed = false;
    std::unique_ptr<ResourceManagerInterface> manager = std::make_unique<TrackedManager>(destroyed);
    EXPECT_FALSE(destroyed);
    manager.reset();
    EXPECT_TRUE(destroyed);
}
TEST(ResourceManagerLifetimeTest, MovesTransferOwnershipAndCopiesShareIt) {
    StubResourceManager original;
    original._data = std::make_shared<vsl::_impl::GraphicResourceManager_impl>();
    const auto data = original._data;
    StubResourceManager copied = original;
    EXPECT_EQ(copied._data, data);
    StubResourceManager moved = std::move(original);
    EXPECT_FALSE(original._data);
    EXPECT_EQ(moved._data, data);
    StubResourceManager assigned;
    assigned = std::move(moved);
    EXPECT_FALSE(moved._data);
    EXPECT_EQ(assigned._data, data);
    original = copied;
    EXPECT_EQ(original._data, data);
}
} // namespace
