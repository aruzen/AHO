#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/swapchain_access.hpp>
#include "support/window_fixture.hpp"
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/storage/resource_layout.hpp>
#include <VSL/vulkan/swapchain.hpp>

namespace {
    using VSL_NAMESPACE::lifetime::detail::StartupAccess;
}

namespace {
    using VSL_NAMESPACE::lifetime::detail::SwapchainAccess;
}

namespace {
    namespace l = vsl::lifetime;
    namespace o = vsl::operation;
    namespace s = vsl::storage;

    struct SurfaceSlot {
        alignas(l::SurfaceSlotStorage) std::byte memory[sizeof(l::SurfaceSlotStorage)];
        bool constructed = false;
        ~SurfaceSlot();
        void construct(vsl::impl::VulkanImplAccessor& instance, vsl::impl::OSWindowImpl& window, const o::SurfaceNativeCalls& calls = {});

        vsl::impl::SurfaceImpl& value() { return l::surface_at(memory); }
    };

    struct ScopedSurfaceWindow {
        vsl::PureWindow window{"surface peer", 64, 48};

        ~ScopedSurfaceWindow() { window.close(); }
    };

    struct SurfacePair {
        l::DependencyNode node;
        l::OwnerRef instance;
        std::array<l::OSWindowOwner, 2> windows;
        o::SurfaceNativeCalls calls;
        alignas(l::SurfaceSlotStorage) std::byte memory[2 * sizeof(l::SurfaceSlotStorage)];
        size_t constructed = 0;
        SurfacePair(vsl::VulkanAccessor instance, std::array<l::OSWindowOwner, 2> windows, const o::SurfaceNativeCalls& calls = {});
        ~SurfacePair();
        void clear() noexcept;

        void* slot(size_t index) { return memory + index * sizeof(l::SurfaceSlotStorage); }

        static void VisitDependencies(const void* owner, l::DependencyNode::EdgeVisitor visitor, void* context);
    };

    struct SurfaceProbe {
        unsigned creates = 0, destroys = 0;
        unsigned fail_on = 1;
        bool fail = true;
        static VkResult Create(void* context, VkInstance instance, void* window, VkSurfaceKHR* surface);
        static void Destroy(void* context, VkInstance instance, VkSurfaceKHR surface) noexcept;
    };

    class SurfaceStorageVulkanIntegrationTest : public vsl_test::WindowIntegrationTest {
        bool validate_vulkan() const override { return true; }
    };

    SurfaceSlot::~SurfaceSlot() {
        if (constructed)
            l::destroy_surface(memory);
    }

    void SurfaceSlot::construct(vsl::impl::VulkanImplAccessor& instance, vsl::impl::OSWindowImpl& window, const o::SurfaceNativeCalls& calls) {
        if (constructed)
            throw std::logic_error("Test surface slot is already constructed");
        l::construct_surface(memory, instance, window, calls);
        constructed = true;
    }

    SurfacePair::SurfacePair(vsl::VulkanAccessor source, std::array<l::OSWindowOwner, 2> windows, const o::SurfaceNativeCalls& calls)
        : instance(source.owner_ref()), windows(std::move(windows)), calls(calls) {
        try {
            while (constructed < 2) {
                const auto index = constructed;
                l::construct_surface(slot(index), *StartupAccess::Owned(source), *l::detail::WindowOwnerAccess::Ref(this->windows[index]), this->calls);
                ++constructed;
                l::set_surface_owners(l::surface_at(slot(index)), instance.alias(StartupAccess::Owned(source).get()), this->windows[index]);
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    SurfacePair::~SurfacePair() {
        node.retire();
        clear();
    }

    void SurfacePair::clear() noexcept {
        while (constructed) {
            auto* memory = slot(--constructed);
            o::destroy_surface(l::surface_at(memory), calls);
            l::destroy_surface(memory);
        }
    }

    void SurfacePair::VisitDependencies(const void* owner, l::DependencyNode::EdgeVisitor visitor, void* context) {
        const auto& root = *static_cast<const SurfacePair*>(owner);
        visitor(context, root.instance);
        for (const auto& window : root.windows) {
            const auto parent = window.owner_ref();
            visitor(context, parent);
        }
    }

    VkResult SurfaceProbe::Create(void* context, VkInstance instance, void* window, VkSurfaceKHR* surface) {
        auto& probe = *static_cast<SurfaceProbe*>(context);
        ++probe.creates;
        if (probe.fail && probe.creates == probe.fail_on) {
            *surface = reinterpret_cast<VkSurfaceKHR>(std::uintptr_t{1});
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        return glfwCreateWindowSurface(instance, static_cast<GLFWwindow*>(window), nullptr, surface);
    }

    void SurfaceProbe::Destroy(void* context, VkInstance instance, VkSurfaceKHR surface) noexcept {
        ++static_cast<SurfaceProbe*>(context)->destroys;
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    TEST(SurfaceStorageTest, ExactLayoutIncludesBothWeakParentContextsAndDisallowsMovingImpls) {
        static_assert(sizeof(vsl::impl::SurfaceImpl) == s::ImplLayout<vsl::impl::SurfaceImpl>::SIZE);
        static_assert(sizeof(l::SurfaceSlotStorage) == s::ResourceSlotLayout<vsl::impl::SurfaceImpl>::SIZE);
        static_assert(alignof(l::SurfaceSlotStorage) == s::ResourceSlotLayout<vsl::impl::SurfaceImpl>::ALIGNMENT);
        static_assert(s::ResourceSlotLayout<vsl::impl::SurfaceImpl>::USE_OFFSET == s::NO_SEPARATE_USE);
        static_assert(!std::is_copy_constructible_v<vsl::impl::SurfaceImpl> && !std::is_move_constructible_v<vsl::impl::SurfaceImpl>);
        EXPECT_GT(sizeof(l::SurfaceSlotStorage), sizeof(vsl::impl::SurfaceImpl));
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, SameTypeSlotsUseDirectOffsetsAndOneOwnerWithOriginalParents) {
        ScopedSurfaceWindow peer;
        auto owner = std::make_shared<SurfacePair>(vsl::lifetime::detail::StartupAccess::FromShared(instance), std::array{window->window_owner(), peer.window.window_owner()});
        owner->node.publish(owner, owner.get(), SurfacePair::VisitDependencies);
        auto* first = &l::surface_at(owner->slot(0));
        auto* second = &l::surface_at(owner->slot(1));
        first->owner_node = second->owner_node = &owner->node;
        auto a = StartupAccess::FromShared(std::shared_ptr<vsl::impl::SurfaceImpl>{owner, first});
        auto b = StartupAccess::FromShared(std::shared_ptr<vsl::impl::SurfaceImpl>{owner, second});
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second) - reinterpret_cast<std::uintptr_t>(first), sizeof(l::SurfaceSlotStorage));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first) % s::SURFACE_SLOT_ALIGNMENT, 0u);
        EXPECT_FALSE(StartupAccess::Owned(a).owner_before(StartupAccess::Owned(b)));
        EXPECT_FALSE(StartupAccess::Owned(b).owner_before(StartupAccess::Owned(a)));
        EXPECT_EQ(StartupAccess::Owned(a.get_instance()), instance);
        EXPECT_EQ(a.get_window_owner().owner_ref(), window->window_owner().owner_ref());
        std::weak_ptr<void> root = owner;
        owner.reset();
        ASSERT_TRUE(window->close());
        ASSERT_TRUE(peer.window.close());
        EXPECT_FALSE(StartupAccess::Owned(physical)->device.getSurfaceFormatsKHR(StartupAccess::Owned(a)->surface).empty());
        StartupAccess::Owned(a).reset();
        EXPECT_FALSE(root.expired());
        StartupAccess::Owned(b).reset();
        EXPECT_TRUE(root.expired());
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, FailedNativeOutputIsNotAdoptedAndTheSameSlotCanBeRetried) {
        const auto os = window->window_owner();
        SurfaceSlot slot;
        SurfaceProbe probe;
        const o::SurfaceNativeCalls calls{&probe, SurfaceProbe::Create, SurfaceProbe::Destroy};
        EXPECT_THROW(slot.construct(*instance, *l::detail::WindowOwnerAccess::Ref(os), calls), vsl::exceptions::RuntimeException);
        EXPECT_EQ(probe.creates, 1u);
        EXPECT_EQ(probe.destroys, 0u);
        probe.fail = false;
        slot.construct(*instance, *l::detail::WindowOwnerAccess::Ref(os), calls);
        EXPECT_TRUE(slot.value().surface);
        EXPECT_THROW(o::initialize_surface(slot.value(), *instance, os.native_handle(), calls), std::logic_error);
        EXPECT_EQ(probe.creates, 2u);
        o::destroy_surface(slot.value(), calls);
        o::destroy_surface(slot.value(), calls);
        EXPECT_EQ(probe.destroys, 1u);
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, GettersKeepTheOriginalParentsWithoutKeepingTheChildAlive) {
        make_surface();
        const auto child = std::weak_ptr<void>(StartupAccess::Shared(*(surface)));
        auto os = surface->get_window_owner();
        auto original = surface->get_instance();
        EXPECT_EQ(StartupAccess::Owned(original), instance);
        EXPECT_FALSE(StartupAccess::Owned(original).owner_before(instance));
        EXPECT_FALSE(instance.owner_before(StartupAccess::Shared(original)));
        const auto parent = os.owner_ref(), expected = window->window_owner().owner_ref();
        EXPECT_FALSE(parent.owner_before(expected));
        EXPECT_FALSE(expected.owner_before(parent));
        ASSERT_TRUE(window->close());
        surface.reset();
        EXPECT_TRUE(child.expired());
        int width = 0, height = 0;
        {
            l::GLFWCallScope scope;
            glfwGetFramebufferSize(static_cast<GLFWwindow*>(os.native_handle()), &width, &height);
        }
        EXPECT_GT(width, 0);
        EXPECT_GT(height, 0);
        EXPECT_TRUE(original.owner_ref().managed());
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, LaterConstructionFailureDestroysOnlySuccessfullyCreatedNativeSurfaces) {
        ScopedSurfaceWindow peer;
        SurfaceProbe probe;
        probe.fail_on = 2;
        const o::SurfaceNativeCalls calls{&probe, SurfaceProbe::Create, SurfaceProbe::Destroy};
        EXPECT_THROW((SurfacePair(vsl::lifetime::detail::StartupAccess::FromShared(instance), std::array{window->window_owner(), peer.window.window_owner()}, calls)), vsl::exceptions::RuntimeException);
        EXPECT_EQ(probe.creates, 2u);
        EXPECT_EQ(probe.destroys, 1u);
        make_surface();
        EXPECT_TRUE(StartupAccess::Owned(*(surface))->surface);
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, DeviceCreationDoesNotPermanentlyRetainTheSurfaceOrItsWindow) {
        make_surface();
        vsl::LogicalDevice logical(physical, surface);
        std::weak_ptr<void> child = StartupAccess::Shared(*(surface));
        std::weak_ptr<void> os = window->window_owner().owner_ref().share();
        ASSERT_TRUE(window->close());
        surface.reset();
        EXPECT_TRUE(child.expired());
        EXPECT_TRUE(os.expired());
        EXPECT_TRUE(StartupAccess::Owned(logical)->device.getQueue(StartupAccess::Owned(logical)->graphics_family, 0));
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, CanonicalSurfaceRejectsMissingOrForeignNodesAndPreservesItsParents) {
        make_surface();
        const auto missing_node = vsl_test::unchecked_startup(StartupAccess::Shared(*surface));
        EXPECT_THROW(missing_node.owner_ref(), std::invalid_argument);
        const auto foreign = vsl_test::unchecked_startup(std::shared_ptr<vsl::impl::SurfaceImpl>(std::make_shared<int>(0), StartupAccess::Owned(*surface).get()));
        EXPECT_THROW(foreign.get_instance(), std::invalid_argument);
        EXPECT_THROW(StartupAccess::FromShared(StartupAccess::Shared(foreign)), std::invalid_argument);
        l::DependencyNode different_node;
        different_node.publish(StartupAccess::Shared(*surface));
        EXPECT_THROW(StartupAccess::FromOwned(different_node.ref(StartupAccess::Shared(*surface))), std::invalid_argument);
        l::DependencyNode different_parent_node;
        different_parent_node.publish(instance);
        EXPECT_THROW(l::set_surface_owners(*StartupAccess::Owned(*surface), different_parent_node.ref(instance), window->window_owner()), std::invalid_argument);
        EXPECT_EQ(StartupAccess::Owned(surface->get_instance()), instance);
        const auto actual_window = surface->get_window_owner(), expected_window = window->window_owner();
        EXPECT_EQ(actual_window.native_handle(), expected_window.native_handle());
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, WeakSurfaceObservesStorageAfterLogicalCloseAndRejectsRetiredOperations) {
        vsl::WeakSurface weak;
        EXPECT_TRUE(weak.expired());
        EXPECT_FALSE(weak.lock());
        make_surface();
        weak = surface->weak();
        auto locked = weak.lock();
        EXPECT_EQ(StartupAccess::Owned(locked), StartupAccess::Owned(*surface));
        EXPECT_FALSE(StartupAccess::Owned(locked).owner_before(StartupAccess::Owned(*surface)));
        const auto native = StartupAccess::Owned(locked)->surface;
        ASSERT_TRUE(window->close());
        surface.reset();
        EXPECT_FALSE(weak.expired());
        EXPECT_TRUE(locked.get_window_owner());
        StartupAccess::Owned(locked)->owner_node->retire();
        auto retired = weak.lock();
        EXPECT_EQ(StartupAccess::Owned(retired), StartupAccess::Owned(locked));
        EXPECT_THROW(retired.owner_ref(), std::invalid_argument);
        EXPECT_THROW(retired.get_instance(), std::invalid_argument);
        EXPECT_THROW(StartupAccess::FromOwned(StartupAccess::Owned(retired)), std::invalid_argument);
        EXPECT_EQ(StartupAccess::Owned(retired)->surface, native);
        locked = {};
        EXPECT_FALSE(weak.expired());
        retired = {};
        EXPECT_TRUE(weak.expired());
        EXPECT_FALSE(weak.lock());
    }

    TEST_F(SurfaceStorageVulkanIntegrationTest, SurfaceAndSavedSwapchainKeepTheNativeWindowAfterLogicalClose) {
        make_surface();
        auto saved = vsl::Swapchain(device, surface);
        std::weak_ptr<void> os = window->window_owner().owner_ref().share();
        std::weak_ptr<void> child = StartupAccess::Shared(*(surface));
        const auto native_surface = SwapchainAccess::Owned(saved)->surface;
        ASSERT_TRUE(window->close());
        surface.reset();
        EXPECT_FALSE(child.expired());
        EXPECT_FALSE(os.expired());
        EXPECT_FALSE(StartupAccess::Owned(physical)->device.getSurfaceFormatsKHR(native_surface->surface).empty());
        auto source = std::make_shared<vsl::Surface>(saved.get_surface());
        vsl::Swapchain next(device, source, saved);
        EXPECT_GT(SwapchainAccess::Owned(next)->swap_chain_extent.width, 0u);
        EXPECT_GT(SwapchainAccess::Owned(next)->swap_chain_extent.height, 0u);
    }
} // namespace
