#include "support/window_fixture.hpp"
#include <VSL/lifetime/surface_storage.hpp>
#include <VSL/storage/resource_layout.hpp>
#include <VSL/vulkan/swapchain.hpp>

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
                l::construct_surface(slot(index), *source.accessor, *l::detail::WindowOwnerAccess::Ref(this->windows[index]), this->calls);
                ++constructed;
                l::set_surface_owners(l::surface_at(slot(index)), instance.alias(source.accessor.get()), this->windows[index]);
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
        auto owner = std::make_shared<SurfacePair>(vsl::VulkanAccessor{instance}, std::array{window->window_owner(), peer.window.window_owner()});
        owner->node.publish(owner, owner.get(), SurfacePair::VisitDependencies);
        auto* first = &l::surface_at(owner->slot(0));
        auto* second = &l::surface_at(owner->slot(1));
        first->owner_node = second->owner_node = &owner->node;
        vsl::Surface a({owner, first}), b({owner, second});
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second) - reinterpret_cast<std::uintptr_t>(first), sizeof(l::SurfaceSlotStorage));
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first) % s::SURFACE_SLOT_ALIGNMENT, 0u);
        EXPECT_FALSE(a.data_.owner_before(b.data_));
        EXPECT_FALSE(b.data_.owner_before(a.data_));
        EXPECT_EQ(a.get_instance().accessor, instance);
        EXPECT_EQ(a.get_window_owner().owner_ref(), window->window_owner().owner_ref());
        std::weak_ptr<void> root = owner;
        owner.reset();
        ASSERT_TRUE(window->close());
        ASSERT_TRUE(peer.window.close());
        EXPECT_FALSE(physical.data_->device.getSurfaceFormatsKHR(a.data_->surface).empty());
        a.data_.reset();
        EXPECT_FALSE(root.expired());
        b.data_.reset();
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
        const auto child = std::weak_ptr<void>(surface->data_);
        auto os = surface->get_window_owner();
        auto original = surface->get_instance();
        EXPECT_EQ(original.accessor, instance);
        EXPECT_FALSE(original.accessor.owner_before(instance));
        EXPECT_FALSE(instance.owner_before(original.accessor));
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
        EXPECT_THROW((SurfacePair(vsl::VulkanAccessor{instance}, std::array{window->window_owner(), peer.window.window_owner()}, calls)), vsl::exceptions::RuntimeException);
        EXPECT_EQ(probe.creates, 2u);
        EXPECT_EQ(probe.destroys, 1u);
        make_surface();
        EXPECT_TRUE(surface->data_->surface);
    }
    TEST_F(SurfaceStorageVulkanIntegrationTest, DeviceCreationDoesNotPermanentlyRetainTheSurfaceOrItsWindow) {
        make_surface();
        vsl::LogicalDevice logical(physical, surface);
        std::weak_ptr<void> child = surface->data_;
        std::weak_ptr<void> os = window->window_owner().owner_ref().share();
        ASSERT_TRUE(window->close());
        surface.reset();
        EXPECT_TRUE(child.expired());
        EXPECT_TRUE(os.expired());
        EXPECT_TRUE(logical.data_->device.getQueue(logical.data_->graphics_family, 0));
    }
    TEST_F(SurfaceStorageVulkanIntegrationTest, SurfaceAndSavedSwapchainKeepTheNativeWindowAfterLogicalClose) {
        make_surface();
        auto saved = vsl::Swapchain(device, surface);
        std::weak_ptr<void> os = window->window_owner().owner_ref().share();
        std::weak_ptr<void> child = surface->data_;
        const auto native_surface = saved.data_->surface;
        ASSERT_TRUE(window->close());
        surface.reset();
        EXPECT_FALSE(child.expired());
        EXPECT_FALSE(os.expired());
        EXPECT_FALSE(physical.data_->device.getSurfaceFormatsKHR(native_surface->surface).empty());
        auto source = std::make_shared<vsl::Surface>(native_surface);
        vsl::Swapchain next(device, source, saved);
        EXPECT_GT(next.data_->swap_chain_extent.width, 0u);
        EXPECT_GT(next.data_->swap_chain_extent.height, 0u);
    }
} // namespace
