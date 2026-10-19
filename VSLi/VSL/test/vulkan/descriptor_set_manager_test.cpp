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
        EXPECT_NE(pools[0]._data, pools[1]._data);
        EXPECT_EQ(exact[0].getPool()._data, pools[0]._data);
        EXPECT_EQ(exact[1].getPool()._data, pools[1]._data);
        for (std::size_t i = 0; i < exact.size(); ++i) {
            exact[i].update(&*output);
            resource = exact[i];
            expect_sequence(execute(43 + static_cast<std::uint32_t>(i)), 43 + static_cast<std::uint32_t>(i));
        }
        for (auto &entry : pools)
            entry._data->resources.clear();
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, DefaultOverAllocatorStartsWithoutPools) {
        vsl::graphic_resource::OverAllocateManager manager(device);
        EXPECT_EQ(manager._data->device, device._data);
        EXPECT_TRUE(manager.pools.empty());
        EXPECT_EQ(manager.other, 0u);
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest,
           OverAllocateReturnsOnePoolPerResourceAndReusesCapacity) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::StorageBuffer, 4}}, 0);
        struct ClearCycles {
            std::shared_ptr<vsl::_impl::GraphicResourceManager_impl> data;
            ~ClearCycles() {
                for (auto &owned : data->pools)
                    owned->resources.clear();
            }
        } cleanup{manager._data};
        auto [pools, allocated] = manager.allocate(std::vector{*binding});
        EXPECT_EQ(pools.size(), allocated.size());
        ASSERT_EQ(allocated.size(), 1u);
        ASSERT_EQ(pools.size(), 1u);
        EXPECT_EQ(pools.front()._data, manager.pools.front()._data);
        EXPECT_EQ(allocated.front().getPool()._data, pools.front()._data);
        resource = allocated.front();
        allocated.front().update(&*output);
        expect_sequence(execute(89), 89);
        auto [again_pools, again] = manager.allocate(std::vector{*binding});
        ASSERT_EQ(again_pools.size(), 1u);
        ASSERT_EQ(again.size(), 1u);
        EXPECT_EQ(again_pools.front()._data, pools.front()._data);
        EXPECT_EQ(manager.pools.size(), 1u);
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
    TEST_F(DescriptorManagerVulkanIntegrationTest, OverAllocatorDestroyReleasesItsOwnedPool) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::StorageBuffer, 1}}, 0);
        ASSERT_EQ(manager.pools.size(), 1u);
        auto allocated_pool = manager.pools.front();
        manager.destroy(allocated_pool);
        EXPECT_TRUE(manager.pools.empty());
        EXPECT_TRUE(manager._data->pools.empty());
        EXPECT_FALSE(allocated_pool._data->descriptorPool);
        manager.destroy(allocated_pool);
        EXPECT_FALSE(allocated_pool._data->descriptorPool);
    }
} // namespace

namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, ExactDestroyInvalidatesTheHandleAndBreaksItsResourceCycle) {
        auto [owned, allocated] = resources->make(std::vector{*binding});
        std::weak_ptr<vsl::_impl::GraphicResourcePool_impl> weak_pool = owned._data;
        std::weak_ptr<vsl::_impl::GraphicResource_impl> weak_resource = allocated.front()._data;
        resources->destroy(owned);
        EXPECT_FALSE(owned._data->descriptorPool);
        EXPECT_TRUE(owned._data->resources.empty());
        // Keep failure diagnostics safe on the old implementation, whose Vulkan pool is already destroyed.
        owned._data->descriptorPool = nullptr;
        owned._data->resources.clear();
        resources->destroy(owned);
        allocated.clear();
        EXPECT_TRUE(weak_resource.expired());
        owned._data.reset();
        EXPECT_TRUE(weak_pool.expired());
        expect_sequence(execute(131), 131);
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, DestroyAcceptsCacheAliasesAndEmptyPoolsWithoutDanglingReferences) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::StorageBuffer, 1}});
        auto retained = manager.pools.front();
        manager.destroy(manager.pools.front());
        EXPECT_TRUE(manager.pools.empty());
        EXPECT_TRUE(manager._data->pools.empty());
        EXPECT_FALSE(retained._data->descriptorPool);
        manager.destroy(Pool{});
        resources->destroy(Pool{});
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, DestroyLeavesAnotherManagersPoolIntact) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::StorageBuffer, 1}});
        auto foreign = manager.pools.front();
        resources->destroy(foreign);
        EXPECT_TRUE(static_cast<bool>(foreign._data->descriptorPool));
        EXPECT_EQ(manager.pools.front()._data, foreign._data);
        EXPECT_EQ(manager._data->pools.size(), 1u);
        struct ClearCycles {
            std::shared_ptr<vsl::_impl::GraphicResourceManager_impl> data;
            ~ClearCycles() {
                for (auto &owned : data->pools)
                    owned->resources.clear();
            }
        } cleanup{manager._data};
        auto [success, allocated] = foreign.bind(*binding);
        ASSERT_TRUE(success);
        allocated.update(&*output, 0, Type::StorageBuffer);
        resource = allocated;
        expect_sequence(execute(137), 137);
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

namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, LazyOverAllocatorReturnsItsNewPoolAndCachesItForReuse) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device);
        struct ClearCycles {
            std::shared_ptr<vsl::_impl::GraphicResourceManager_impl> data;
            ~ClearCycles() {
                for (auto &owned : data->pools)
                    owned->resources.clear();
            }
        } cleanup{manager._data};
        auto [pools, allocated] = manager.allocate(std::vector{*binding});
        ASSERT_EQ(pools.size(), 1u);
        ASSERT_EQ(allocated.size(), 1u);
        ASSERT_EQ(manager.pools.size(), 1u);
        EXPECT_EQ(pools.front()._data, manager.pools.front()._data);
        EXPECT_EQ(allocated.front().getPool()._data, pools.front()._data);
        allocated.front().update(&*output);
        resource = allocated.front();
        expect_sequence(execute(97), 97);
        auto [empty_pools, empty_resources] = manager.allocate(std::vector<BindingLayout>{});
        EXPECT_TRUE(empty_pools.empty());
        EXPECT_TRUE(empty_resources.empty());
        EXPECT_EQ(manager.pools.size(), 1u);
    }
} // namespace

#include <atomic>
#include <iostream>
#include <limits>
namespace {
    class DescriptorPoolValidationVulkanIntegrationTest : public vsl_test::VulkanIntegrationTest {
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger = nullptr;
        std::atomic<unsigned> validation_errors{0};
        static VKAPI_ATTR VkBool32 VKAPI_CALL record_error(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                           VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *message, void *user) {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                ++*static_cast<std::atomic<unsigned> *>(user);
                std::cerr << message->pMessageIdName << ": " << message->pMessage << '\n';
                return VK_TRUE; // Abort an invalid Vulkan call before it reaches the driver.
            }
            return VK_FALSE;
        }
        void SetUp() override {
            require_validation = true;
            VulkanIntegrationTest::SetUp();
            if (HasFatalFailure() || IsSkipped())
                return;
            const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                instance->instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
            destroy_messenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                instance->instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
            ASSERT_NE(create, nullptr);
            ASSERT_NE(destroy_messenger, nullptr);
            VkDebugUtilsMessengerCreateInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            info.pfnUserCallback = record_error;
            info.pUserData = &validation_errors;
            ASSERT_EQ(create(instance->instance, &info, nullptr, &messenger), VK_SUCCESS);
        }
        void TearDown() override {
            EXPECT_EQ(validation_errors.load(), 0u) << "Descriptor pool operations emitted validation errors";
            if (messenger)
                destroy_messenger(instance->instance, messenger, nullptr);
            VulkanIntegrationTest::TearDown();
        }
    };
    TEST_F(DescriptorPoolValidationVulkanIntegrationTest, MakeAllocatesAllArrayElementsAcrossLayoutsAndBindings) {
        using namespace vsl::graphic_resource;
        ExactAllocateManager manager(device);
        BindingLayout first(device, {{0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 4},
                                     {3, Type::UniformBuffer, vsl::ShaderFlag::Compute, 2}});
        BindingLayout second(device, {{1, Type::StorageBuffer, vsl::ShaderFlag::Compute, 3}});
        auto [array_pool, allocated] = manager.make(std::vector{first, second},
                                                    std::map<Type, std::size_t>{{Type::StorageBuffer, 2}, {Type::UniformBuffer, 1}});
        struct ClearResources {
            vsl::GraphicResourcePool pool;
            ~ClearResources() { pool._data->resources.clear(); }
        } cleanup{array_pool};
        ASSERT_EQ(allocated.size(), 2u);
        EXPECT_EQ(allocated[0].getPool()._data, array_pool._data);
        EXPECT_EQ(allocated[1].getPool()._data, array_pool._data);
        EXPECT_TRUE(static_cast<bool>(allocated[0]._data->descriptorSet));
        EXPECT_TRUE(static_cast<bool>(allocated[1]._data->descriptorSet));
        BindingLayout spare(device, {{0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 2},
                                     {1, Type::UniformBuffer, vsl::ShaderFlag::Compute, 1}});
        auto [success, additional] = array_pool.bind(spare);
        ASSERT_TRUE(success); // The explicit counts are extra capacity beyond the layouts.
        EXPECT_EQ(additional.getPool()._data, array_pool._data);
    }
} // namespace

namespace {
    TEST_F(DescriptorPoolValidationVulkanIntegrationTest, RejectsLayoutTotalsBeyondNativeCapacityBeforeCreatingAPool) {
        using namespace vsl::graphic_resource;
        ExactAllocateManager manager(device);
        BindingLayout array(device, 0, Type::StorageBuffer, vsl::ShaderFlag::Compute, 4);
        EXPECT_THROW(manager.make(std::vector{array},
                                  std::map<Type, std::size_t>{{Type::StorageBuffer, UINT32_MAX}}),
                     vsl::exceptions::RuntimeException);
        EXPECT_TRUE(manager._data->pools.empty());
        for (auto &pool : manager._data->pools)
            pool->resources.clear(); // Clean up a pool created by the old implementation, too.
    }
#if SIZE_MAX > UINT32_MAX
    TEST_F(DescriptorPoolValidationVulkanIntegrationTest, RejectsOversizedExplicitAndFallbackCountsBeforeCreatingAPool) {
        using namespace vsl::graphic_resource;
        ExactAllocateManager manager(device);
        const auto overflow = std::size_t{UINT32_MAX} + 1;
        EXPECT_THROW(manager.make(std::map<Type, std::size_t>{{Type::StorageBuffer, overflow}}),
                     vsl::exceptions::RuntimeException);
        EXPECT_THROW(manager.make(std::map<Type, std::size_t>{}, overflow), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(manager._data->pools.empty());
    }
#endif
} // namespace

namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, EmptyExactAllocationCreatesNoPoolOrResource) {
        const auto previous = resources->_data->pools.size();
        auto [pools, allocated] = resources->allocate(std::vector<vsl::graphic_resource::BindingLayout>{});
        EXPECT_TRUE(pools.empty());
        EXPECT_TRUE(allocated.empty());
        EXPECT_EQ(resources->_data->pools.size(), previous);
    }
} // namespace
