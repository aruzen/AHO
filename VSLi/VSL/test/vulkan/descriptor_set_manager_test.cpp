#include <VSL/lifetime/descriptor_access.hpp>
#include "support/compute_fixture.hpp"
#include "support/deferred_action.hpp"

namespace {
    using VSL_NAMESPACE::lifetime::detail::DescriptorAccess;
    using VSL_NAMESPACE::lifetime::detail::DescriptorManagerAccess;
} // namespace
namespace {
    using DescriptorManagerVulkanIntegrationTest = vsl_test::ComputeVulkanIntegrationTest;
    TEST_F(DescriptorManagerVulkanIntegrationTest, OverAssignmentPublishesConfigurationAndBothCachedPoolsBeforeReleasingOldOwners) {
        using namespace vsl::graphic_resource;
        struct Context {
            Pool source;
            std::shared_ptr<void> action;
        };
        for (const bool moving : {false, true}) {
            OverAllocateManager target(device, {{Type::STORAGE_BUFFER, 1}});
            OverAllocateManager source(device, {{Type::STORAGE_BUFFER, 3}});
            source.pools.push_back(source.make({{Type::UNIFORM_BUFFER, 1}}));
            target.other = 4;
            source.other = 9;
            const auto expected_state = DescriptorManagerAccess::Data(source);
            const auto expected_size = source.size;
            const auto expected_pools = source.pools;
            auto previous = target.pools.front();
            bool called = false;
            vsl_test::DeferredAction release([&] {
                called = true;
                EXPECT_EQ(previous.get_manager(), nullptr);
                EXPECT_EQ(DescriptorManagerAccess::Data(target), expected_state);
                EXPECT_EQ(target.size, expected_size);
                EXPECT_EQ(target.other, 9u);
                EXPECT_EQ(target.pools.size(), expected_pools.size());
                if (target.pools.size() == expected_pools.size())
                    for (size_t i = 0; i < expected_pools.size(); ++i)
                        EXPECT_EQ(DescriptorAccess::Owned(target.pools[i]), DescriptorAccess::Owned(expected_pools[i]));
            });
            {
                auto context = std::make_shared<Context>(Context{previous, release.owner()});
                target.pools.front() = DescriptorAccess::FromShared(std::shared_ptr<vsl::impl::GraphicResourcePoolImpl>(context, DescriptorAccess::Owned(previous).get()));
            }
            if (moving)
                target = std::move(source);
            else
                target = source;
            EXPECT_TRUE(called);
            EXPECT_EQ(DescriptorAccess::Owned(target.pools.front()), DescriptorAccess::Owned(expected_pools.front()));
        }
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, OverDestructionInvalidatesItsIdentityBeforeCachedOwnersRunTheirCallbacks) {
        using namespace vsl::graphic_resource;
        struct Context {
            Pool source;
            std::shared_ptr<void> action;
        };
        auto target = std::make_unique<OverAllocateManager>(device, std::map<Type, size_t>{{Type::STORAGE_BUFFER, 1}});
        auto previous = target->pools.front();
        bool called = false;
        vsl_test::DeferredAction release([&] {
            called = true;
            EXPECT_EQ(previous.get_manager(), nullptr);
        });
        {
            auto context = std::make_shared<Context>(Context{previous, release.owner()});
            target->pools.front() = DescriptorAccess::FromShared(std::shared_ptr<vsl::impl::GraphicResourcePoolImpl>(context, DescriptorAccess::Owned(previous).get()));
        }
        target.reset();
        EXPECT_TRUE(called);
        EXPECT_EQ(previous.get_manager(), nullptr);
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, AssignmentPublishesStateAndIdentityBeforeTheOldOwnerReentersTheManager) {
        using namespace vsl::graphic_resource;
        struct Context {
            std::shared_ptr<vsl::impl::GraphicResourceManagerImpl> state;
            std::shared_ptr<void> action;
        };
        for (const bool moving : {false, true}) {
            ExactAllocateManager target(device), source(device);
            auto previous = target.make({{Type::STORAGE_BUFFER, 1}});
            std::optional<Pool> created;
            ManagerInterface* observed = nullptr;
            std::weak_ptr<void> released;
            vsl_test::DeferredAction release([&] {
                observed = previous.get_manager();
                created.emplace(target.make({{Type::STORAGE_BUFFER, 1}}));
            });
            {
                const auto state = DescriptorManagerAccess::Data(target);
                auto context = std::make_shared<Context>(Context{state, release.owner()});
                released = context;
                DescriptorManagerAccess::Data(target) = {context, state.get()};
            }
            if (moving)
                target = std::move(source);
            else
                target = source;
            EXPECT_EQ(observed, nullptr);
            ASSERT_TRUE(created);
            EXPECT_EQ(created->get_manager(), &target);
            EXPECT_TRUE(released.expired());
        }
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, MakeAndExactAllocateReturnUsableResources) {
        auto [allocated_pool, allocated] = resources->make(std::vector{*binding});
        ASSERT_EQ(allocated.size(), 1u);
        EXPECT_EQ(allocated_pool.get_manager(), &*resources);
        allocated.front().update(&*output);
        resource = allocated.front();
        ExpectSequence(execute(41), 41);
        DescriptorAccess::Owned(allocated_pool)->resources.clear();
        auto [pools, exact] = resources->allocate(std::vector{*binding, *binding});
        ASSERT_EQ(pools.size(), 2u);
        ASSERT_EQ(exact.size(), 2u);
        EXPECT_NE(DescriptorAccess::Owned(pools[0]), DescriptorAccess::Owned(pools[1]));
        EXPECT_EQ(DescriptorAccess::Owned(exact[0].get_pool()), DescriptorAccess::Owned(pools[0]));
        EXPECT_EQ(DescriptorAccess::Owned(exact[1].get_pool()), DescriptorAccess::Owned(pools[1]));
        for (std::size_t i = 0; i < exact.size(); ++i) {
            exact[i].update(&*output);
            resource = exact[i];
            ExpectSequence(execute(43 + static_cast<std::uint32_t>(i)), 43 + static_cast<std::uint32_t>(i));
        }
        for (auto& entry : pools)
            DescriptorAccess::Owned(entry)->resources.clear();
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, DefaultOverAllocatorStartsWithoutPools) {
        vsl::graphic_resource::OverAllocateManager manager(device);
        EXPECT_EQ(DescriptorManagerAccess::Data(manager)->device, device.data_);
        EXPECT_TRUE(manager.pools.empty());
        EXPECT_EQ(manager.other, 0u);
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest,
           OverAllocateReturnsOnePoolPerResourceAndReusesCapacity) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::STORAGE_BUFFER, 4}}, 0);
        struct ClearCycles {
            std::shared_ptr<vsl::impl::GraphicResourceManagerImpl> data;
            ~ClearCycles() {
                for (auto& owned : data->pools)
                    owned->resources.clear();
            }
        } cleanup{DescriptorManagerAccess::Data(manager)};
        auto [pools, allocated] = manager.allocate(std::vector{*binding});
        EXPECT_EQ(pools.size(), allocated.size());
        ASSERT_EQ(allocated.size(), 1u);
        ASSERT_EQ(pools.size(), 1u);
        EXPECT_EQ(DescriptorAccess::Owned(pools.front()), DescriptorAccess::Owned(manager.pools.front()));
        EXPECT_EQ(DescriptorAccess::Owned(allocated.front().get_pool()), DescriptorAccess::Owned(pools.front()));
        resource = allocated.front();
        allocated.front().update(&*output);
        ExpectSequence(execute(89), 89);
        auto [again_pools, again] = manager.allocate(std::vector{*binding});
        ASSERT_EQ(again_pools.size(), 1u);
        ASSERT_EQ(again.size(), 1u);
        EXPECT_EQ(DescriptorAccess::Owned(again_pools.front()), DescriptorAccess::Owned(pools.front()));
        EXPECT_EQ(manager.pools.size(), 1u);
        for (auto& entry : DescriptorManagerAccess::Data(manager)->pools)
            entry->resources.clear();
    }
} // namespace
namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, TemplateMakeAndAllocateOverloadsUpdateTheirBuffers) {
        auto& manager = static_cast<vsl::graphic_resource::ManagerInterface&>(*resources);
        auto use = [&](auto allocated, std::uint32_t base) {
            ASSERT_EQ(allocated.size(), 1u);
            resource = allocated.front();
            ExpectSequence(execute(base), base);
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
        OverAllocateManager manager(device, {{Type::STORAGE_BUFFER, 1}}, 0);
        ASSERT_EQ(manager.pools.size(), 1u);
        auto allocated_pool = manager.pools.front();
        manager.destroy(allocated_pool);
        EXPECT_TRUE(manager.pools.empty());
        EXPECT_TRUE(DescriptorManagerAccess::Data(manager)->pools.empty());
        EXPECT_FALSE(DescriptorAccess::Owned(allocated_pool)->descriptor_pool);
        manager.destroy(allocated_pool);
        EXPECT_FALSE(DescriptorAccess::Owned(allocated_pool)->descriptor_pool);
    }
} // namespace

namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, ExactDestroyInvalidatesTheHandleAndBreaksItsResourceCycle) {
        auto [owned, allocated] = resources->make(std::vector{*binding});
        std::weak_ptr<vsl::impl::GraphicResourcePoolImpl> weak_pool = DescriptorAccess::Owned(owned).share();
        std::weak_ptr<vsl::impl::GraphicResourceImpl> weak_resource = DescriptorAccess::Owned(allocated.front()).share();
        resources->destroy(owned);
        EXPECT_FALSE(DescriptorAccess::Owned(owned)->descriptor_pool);
        EXPECT_TRUE(DescriptorAccess::Owned(owned)->resources.empty());
        // Keep failure diagnostics safe on the old implementation, whose Vulkan pool is already destroyed.
        DescriptorAccess::Owned(owned)->descriptor_pool = nullptr;
        DescriptorAccess::Owned(owned)->resources.clear();
        resources->destroy(owned);
        allocated.clear();
        EXPECT_TRUE(weak_resource.expired());
        DescriptorAccess::Owned(owned).reset();
        EXPECT_TRUE(weak_pool.expired());
        ExpectSequence(execute(131), 131);
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, DestroyAcceptsCacheAliasesAndEmptyPoolsWithoutDanglingReferences) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::STORAGE_BUFFER, 1}});
        auto retained = manager.pools.front();
        manager.destroy(manager.pools.front());
        EXPECT_TRUE(manager.pools.empty());
        EXPECT_TRUE(DescriptorManagerAccess::Data(manager)->pools.empty());
        EXPECT_FALSE(DescriptorAccess::Owned(retained)->descriptor_pool);
        manager.destroy(Pool{});
        resources->destroy(Pool{});
    }
    TEST_F(DescriptorManagerVulkanIntegrationTest, DestroyLeavesAnotherManagersPoolIntact) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device, {{Type::STORAGE_BUFFER, 1}});
        auto foreign = manager.pools.front();
        resources->destroy(foreign);
        EXPECT_TRUE(static_cast<bool>(DescriptorAccess::Owned(foreign)->descriptor_pool));
        EXPECT_EQ(DescriptorAccess::Owned(manager.pools.front()), DescriptorAccess::Owned(foreign));
        EXPECT_EQ(DescriptorManagerAccess::Data(manager)->pools.size(), 1u);
        struct ClearCycles {
            std::shared_ptr<vsl::impl::GraphicResourceManagerImpl> data;
            ~ClearCycles() {
                for (auto& owned : data->pools)
                    owned->resources.clear();
            }
        } cleanup{DescriptorManagerAccess::Data(manager)};
        auto [success, allocated] = foreign.bind(*binding);
        ASSERT_TRUE(success);
        allocated.update(&*output, 0, Type::STORAGE_BUFFER);
        resource = allocated;
        ExpectSequence(execute(137), 137);
    }
} // namespace

#include <type_traits>
namespace {
    using ResourceManagerInterface = vsl::graphic_resource::ManagerInterface;
    static_assert(std::has_virtual_destructor_v<ResourceManagerInterface>);
    struct StubResourceManager : ResourceManagerInterface {
        std::tuple<std::vector<vsl::graphic_resource::Pool>, std::vector<vsl::graphic_resource::Resource>>
        allocate(std::vector<vsl::graphic_resource::BindingLayout>) override { return {}; }
        void destroy(const vsl::graphic_resource::Pool&) override {}
    };
    TEST(ResourceManagerLifetimeTest, BaseOwnerRunsTheDerivedDestructor) {
        struct TrackedManager : StubResourceManager {
            bool& destroyed;
            explicit TrackedManager(bool& destroyed) : destroyed(destroyed) {}
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
        DescriptorManagerAccess::Data(original) = std::make_shared<vsl::impl::GraphicResourceManagerImpl>();
        const auto data = DescriptorManagerAccess::Data(original);
        StubResourceManager copied = original;
        EXPECT_EQ(DescriptorManagerAccess::Data(copied), data);
        StubResourceManager moved = std::move(original);
        EXPECT_FALSE(DescriptorManagerAccess::Data(original));
        EXPECT_EQ(DescriptorManagerAccess::Data(moved), data);
        StubResourceManager assigned;
        assigned = std::move(moved);
        EXPECT_FALSE(DescriptorManagerAccess::Data(moved));
        EXPECT_EQ(DescriptorManagerAccess::Data(assigned), data);
        original = copied;
        EXPECT_EQ(DescriptorManagerAccess::Data(original), data);
    }
} // namespace

namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, LazyOverAllocatorReturnsItsNewPoolAndCachesItForReuse) {
        using namespace vsl::graphic_resource;
        OverAllocateManager manager(device);
        struct ClearCycles {
            std::shared_ptr<vsl::impl::GraphicResourceManagerImpl> data;
            ~ClearCycles() {
                for (auto& owned : data->pools)
                    owned->resources.clear();
            }
        } cleanup{DescriptorManagerAccess::Data(manager)};
        auto [pools, allocated] = manager.allocate(std::vector{*binding});
        ASSERT_EQ(pools.size(), 1u);
        ASSERT_EQ(allocated.size(), 1u);
        ASSERT_EQ(manager.pools.size(), 1u);
        EXPECT_EQ(DescriptorAccess::Owned(pools.front()), DescriptorAccess::Owned(manager.pools.front()));
        EXPECT_EQ(DescriptorAccess::Owned(allocated.front().get_pool()), DescriptorAccess::Owned(pools.front()));
        allocated.front().update(&*output);
        resource = allocated.front();
        ExpectSequence(execute(97), 97);
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
        VkDebugUtilsMessengerEXT _messenger = VK_NULL_HANDLE;
        PFN_vkDestroyDebugUtilsMessengerEXT _destroy_messenger = nullptr;
        std::atomic<unsigned> _validation_errors{0};
        static VKAPI_ATTR VkBool32 VKAPI_CALL RecordError(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                          VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void* user);
        void SetUp() override;
        void TearDown() override {
            EXPECT_EQ(_validation_errors.load(), 0u) << "Descriptor pool operations emitted validation errors";
            if (_messenger)
                _destroy_messenger(instance->instance, _messenger, nullptr);
            VulkanIntegrationTest::TearDown();
        }
    };
    TEST_F(DescriptorPoolValidationVulkanIntegrationTest, MakeAllocatesAllArrayElementsAcrossLayoutsAndBindings) {
        using namespace vsl::graphic_resource;
        ExactAllocateManager manager(device);
        BindingLayout first(device, {{0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 4},
                                     {3, Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE, 2}});
        BindingLayout second(device, {{1, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 3}});
        auto [array_pool, allocated] = manager.make(std::vector{first, second},
                                                    std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, 2}, {Type::UNIFORM_BUFFER, 1}});
        struct ClearResources {
            vsl::GraphicResourcePool pool;
            ~ClearResources() { DescriptorAccess::Owned(pool)->resources.clear(); }
        } cleanup{array_pool};
        ASSERT_EQ(allocated.size(), 2u);
        EXPECT_EQ(DescriptorAccess::Owned(allocated[0].get_pool()), DescriptorAccess::Owned(array_pool));
        EXPECT_EQ(DescriptorAccess::Owned(allocated[1].get_pool()), DescriptorAccess::Owned(array_pool));
        EXPECT_TRUE(static_cast<bool>(DescriptorAccess::Owned(allocated[0])->descriptor_set));
        EXPECT_TRUE(static_cast<bool>(DescriptorAccess::Owned(allocated[1])->descriptor_set));
        BindingLayout spare(device, {{0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 2},
                                     {1, Type::UNIFORM_BUFFER, vsl::ShaderFlag::COMPUTE, 1}});
        auto [success, additional] = array_pool.bind(spare);
        ASSERT_TRUE(success); // The explicit counts are extra capacity beyond the layouts.
        EXPECT_EQ(DescriptorAccess::Owned(additional.get_pool()), DescriptorAccess::Owned(array_pool));
    }
} // namespace

namespace {
    TEST_F(DescriptorPoolValidationVulkanIntegrationTest, RejectsLayoutTotalsBeyondNativeCapacityBeforeCreatingAPool) {
        using namespace vsl::graphic_resource;
        ExactAllocateManager manager(device);
        BindingLayout array(device, 0, Type::STORAGE_BUFFER, vsl::ShaderFlag::COMPUTE, 4);
        EXPECT_THROW(manager.make(std::vector{array},
                                  std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, UINT32_MAX}}),
                     vsl::exceptions::RuntimeException);
        EXPECT_TRUE(DescriptorManagerAccess::Data(manager)->pools.empty());
        for (auto& pool : DescriptorManagerAccess::Data(manager)->pools)
            pool->resources.clear(); // Clean up a pool created by the old implementation, too.
    }
#if SIZE_MAX > UINT32_MAX
    TEST_F(DescriptorPoolValidationVulkanIntegrationTest, RejectsOversizedExplicitAndFallbackCountsBeforeCreatingAPool) {
        using namespace vsl::graphic_resource;
        ExactAllocateManager manager(device);
        const auto overflow = std::size_t{UINT32_MAX} + 1;
        EXPECT_THROW(manager.make(std::map<Type, std::size_t>{{Type::STORAGE_BUFFER, overflow}}),
                     vsl::exceptions::RuntimeException);
        EXPECT_THROW(manager.make(std::map<Type, std::size_t>{}, overflow), vsl::exceptions::RuntimeException);
        EXPECT_TRUE(DescriptorManagerAccess::Data(manager)->pools.empty());
    }
#endif
} // namespace

namespace {
    TEST_F(DescriptorManagerVulkanIntegrationTest, EmptyExactAllocationCreatesNoPoolOrResource) {
        const auto previous = DescriptorManagerAccess::Data(*(resources))->pools.size();
        auto [pools, allocated] = resources->allocate(std::vector<vsl::graphic_resource::BindingLayout>{});
        EXPECT_TRUE(pools.empty());
        EXPECT_TRUE(allocated.empty());
        EXPECT_EQ(DescriptorManagerAccess::Data(*(resources))->pools.size(), previous);
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DescriptorPoolValidationVulkanIntegrationTest::RecordError(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                                                              VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void* user) {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            ++*static_cast<std::atomic<unsigned>*>(user);
            std::cerr << message->pMessageIdName << ": " << message->pMessage << '\n';
            return VK_TRUE; // Abort an invalid Vulkan call before it reaches the driver.
        }
        return VK_FALSE;
    }

    void DescriptorPoolValidationVulkanIntegrationTest::SetUp() {
        require_validation = true;
        VulkanIntegrationTest::SetUp();
        if (HasFatalFailure() || IsSkipped())
            return;
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            instance->instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        _destroy_messenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            instance->instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
        ASSERT_NE(create, nullptr);
        ASSERT_NE(_destroy_messenger, nullptr);
        VkDebugUtilsMessengerCreateInfoEXT info{};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = RecordError;
        info.pUserData = &_validation_errors;
        ASSERT_EQ(create(instance->instance, &info, nullptr, &_messenger), VK_SUCCESS);
    }
} // namespace
