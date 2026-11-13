#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/owner_access.hpp>
#include <VSL/lifetime/physical_device_storage.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    namespace {
        std::vector<PhysicalDevice> enumerate_snapshot(std::shared_ptr<impl::PhysicalDevicesImpl> snapshot);
        std::vector<PhysicalDevice> enumerate_snapshot(std::shared_ptr<impl::PhysicalDevicesImpl> snapshot) {
            if (!snapshot || !snapshot.use_count())
                throw std::invalid_argument("Physical devices snapshot is empty");
            const auto instance = StartupAccess::FromShared(snapshot->vulkan);
            const auto handles = snapshot->devices;
            std::vector<PhysicalDevice> devices;
            devices.reserve(handles.size());
            for (const auto handle : handles) {
                const auto candidate = operation::inspect_physical_device(*StartupAccess::Owned(instance), handle);
                devices.emplace_back(StartupAccess::FromShared(make_heap_physical_device(instance, candidate)));
            }
            return devices;
        }
    } // namespace
    PhysicalDeviceCollection::PhysicalDeviceCollection(VulkanAccessor instance) {
        StartupAccess::RequireCanonical(StartupAccess::Owned(instance));
        auto state = std::make_shared<impl::PhysicalDevicesImpl>();
        state->devices = operation::enumerate_physical_devices(*StartupAccess::Owned(instance));
        state->vulkan = OwnerAccess::Shared(StartupAccess::Owned(instance));
        _state = std::move(state);
    }
    std::vector<PhysicalDevice> PhysicalDeviceCollection::get() const& {
        return enumerate_snapshot(_state);
    }
    std::vector<PhysicalDevice> PhysicalDeviceCollection::get() && {
        return enumerate_snapshot(std::move(_state));
    }
    PhysicalDevice PhysicalDeviceCollection::select() const {
        const auto snapshot = _state;
        if (!snapshot || !snapshot.use_count())
            throw std::invalid_argument("Physical devices snapshot is empty");
        const auto instance = StartupAccess::FromShared(snapshot->vulkan);
        const auto handles = snapshot->devices;
        const auto selected = operation::select_physical_device(*StartupAccess::Owned(instance), handles);
        return StartupAccess::FromShared(make_heap_physical_device(instance, selected));
    }
} // namespace VSL_NAMESPACE::lifetime::detail
