#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/vulkan/_pimpls.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime::detail {
    namespace {
        template<typename T>
        void require_canonical(const OwnedRef<T>& owner);
        template<typename T>
        OwnedRef<T> canonical_shared(std::shared_ptr<T> owner);

        template<typename T>
        void require_canonical(const OwnedRef<T>& owner) {
            const auto* node = OwnerAccess::Node(owner);
            if (!owner || !owner.use_count() || !node || node != owner->owner_node ||
                node->retiring() || !node->matches(OwnerAccess::Shared(owner)))
                throw std::invalid_argument("Startup resource requires its live canonical owner");
        }

        template<typename T>
        OwnedRef<T> canonical_shared(std::shared_ptr<T> owner) {
            if (!owner || !owner.use_count() || !owner->owner_node || !owner->owner_node->matches(owner))
                throw std::invalid_argument("Startup resource requires its canonical owner");
            const auto* node = owner->owner_node;
            return node->ref(std::move(owner));
        }
    } // namespace

    void StartupAccess::RequireCanonical(const OwnedRef<impl::VulkanImplAccessor>& owner) {
        require_canonical(owner);
    }

    void StartupAccess::RequireCanonical(const OwnedRef<impl::PhysicalDeviceImpl>& owner) {
        require_canonical(owner);
    }

    VulkanAccessor StartupAccess::FromOwned(OwnedRef<impl::VulkanImplAccessor> owner) {
        RequireCanonical(owner);
        return VulkanAccessor(std::move(owner));
    }

    PhysicalDevice StartupAccess::FromOwned(OwnedRef<impl::PhysicalDeviceImpl> owner) {
        RequireCanonical(owner);
        return PhysicalDevice(std::move(owner));
    }

    VulkanAccessor StartupAccess::FromShared(std::shared_ptr<impl::VulkanImplAccessor> owner) {
        return FromOwned(canonical_shared(std::move(owner)));
    }

    PhysicalDevice StartupAccess::FromShared(std::shared_ptr<impl::PhysicalDeviceImpl> owner) {
        return FromOwned(canonical_shared(std::move(owner)));
    }

    void StartupAccess::RequireCanonical(const OwnedRef<impl::LogicalDeviceImpl>& owner) {
        require_canonical(owner);
        if (!owner->device)
            throw std::invalid_argument("LogicalDeviceAccessor requires a live native handle");
    }

    LogicalDeviceAccessor StartupAccess::FromOwned(OwnedRef<impl::LogicalDeviceImpl> owner) {
        RequireCanonical(owner);
        return LogicalDeviceAccessor(std::move(owner));
    }

    LogicalDeviceAccessor StartupAccess::FromShared(std::shared_ptr<impl::LogicalDeviceImpl> owner) {
        return FromOwned(canonical_shared(std::move(owner)));
    }

    void StartupAccess::RequireCanonical(const OwnedRef<impl::SurfaceImpl>& owner) {
        require_canonical(owner);
        if (!owner->surface)
            throw std::invalid_argument("Surface requires a live native handle");
    }

    Surface StartupAccess::FromOwned(OwnedRef<impl::SurfaceImpl> owner) {
        RequireCanonical(owner);
        return Surface(std::move(owner));
    }

    Surface StartupAccess::FromShared(std::shared_ptr<impl::SurfaceImpl> owner) {
        return FromOwned(canonical_shared(std::move(owner)));
    }
} // namespace VSL_NAMESPACE::lifetime::detail
