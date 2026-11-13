#pragma once
#include "../define.hpp"
#include "vulkan.hpp"
#include "surface.hpp"
#include "../operation/device_create_info.hpp"

#include "../type.hpp"
#include "../concepts.hpp"

#include <any>
#include <optional>
#include <utility>
#include "../exceptions.hpp"

namespace VSL_NAMESPACE {
    struct Surface;

    namespace physical_device_type {
        class PhysicalDeviceType : public Type_<PhysicalDeviceType> {
        public:
            consteval PhysicalDeviceType(const char* name, char index) : Type_(name, index) {}
        };
        constexpr PhysicalDeviceType OTHER = PhysicalDeviceType("OTHER", 0);
        constexpr PhysicalDeviceType INTEGRATED_GPU = PhysicalDeviceType("INTEGRATED_GPU", 1);
        constexpr PhysicalDeviceType DISCRETE_GPU = PhysicalDeviceType("DISCRETE_GPU", 2);
        constexpr PhysicalDeviceType VIRTUAL_GPU = PhysicalDeviceType("VIRTUAL_GPU", 3);
        constexpr PhysicalDeviceType CPU = PhysicalDeviceType("CPU", 4);

        PhysicalDeviceType get_type(int a);
    }; // namespace physical_device_type

    struct PhysicalDevicesSercher {
        int benchmark(PhysicalDevice device);
    };

    template<bool v>
    struct LogicalDeviceGraphicCreator {
        std::shared_ptr<impl::LogicalDeviceImpl> create(PhysicalDevice device, Surface surface);
    };

    struct PhysicalDevice {
    private:
        lifetime::OwnedRef<impl::PhysicalDeviceImpl> _owner;
        explicit PhysicalDevice(lifetime::OwnedRef<impl::PhysicalDeviceImpl> owner) noexcept : _owner(std::move(owner)) {}
        friend struct lifetime::detail::StartupAccess;

    public:
        PhysicalDevice() = default;
        PhysicalDevice(const PhysicalDevice&) = default;
        PhysicalDevice(PhysicalDevice&&) noexcept = default;
        PhysicalDevice& operator=(PhysicalDevice value) noexcept {
            swap(value);
            return *this;
        }
        void swap(PhysicalDevice& other) noexcept { _owner.swap(other._owner); }
        explicit operator bool() const noexcept { return bool(_owner); }
        ~PhysicalDevice();

        static PhysicalDevice Select(VulkanAccessor instance);
        VulkanAccessor get_instance() const;
        lifetime::OwnerRef owner_ref() const;
        DeviceFeature supported_features() const;
        uint32_t api_version();
        uint32_t driver_version();
        uint32_t vendor_id();
        uint32_t device_id();
        physical_device_type::PhysicalDeviceType type();
        std::string name();
    };

    namespace lifetime::detail {
        class PhysicalDeviceCollection {
            std::shared_ptr<impl::PhysicalDevicesImpl> _state;
            explicit PhysicalDeviceCollection(VulkanAccessor instance);
            std::vector<PhysicalDevice> get() const&;
            std::vector<PhysicalDevice> get() &&;
            PhysicalDevice select() const;
            template<typename>
            friend struct ::VSL_NAMESPACE::PhysicalDevices;
            friend struct StartupAccess;

        public:
            PhysicalDeviceCollection(const PhysicalDeviceCollection&) = default;
            PhysicalDeviceCollection(PhysicalDeviceCollection&&) noexcept = default;
            PhysicalDeviceCollection& operator=(PhysicalDeviceCollection value) noexcept {
                swap(value);
                return *this;
            }
            void swap(PhysicalDeviceCollection& other) noexcept { _state.swap(other._state); }
        };
    } // namespace lifetime::detail

    template<typename S>
    struct PhysicalDevices {
    private:
        lifetime::detail::PhysicalDeviceCollection _collection;
        friend struct lifetime::detail::StartupAccess;

    public:
        using BenchMarcker = S;
        PhysicalDevices(VulkanAccessor vulkan);
        PhysicalDevices(const PhysicalDevices&) = default;
        PhysicalDevices(PhysicalDevices&&) noexcept = default;
        PhysicalDevices& operator=(PhysicalDevices value) noexcept {
            swap(value);
            return *this;
        }
        void swap(PhysicalDevices& other) noexcept { _collection.swap(other._collection); }
        ~PhysicalDevices();
        std::vector<PhysicalDevice> get();
        PhysicalDevice search();
#ifdef VSL_NAMESPACE_TEST
        void test();
#endif
    };

    struct LogicalDeviceAccessor {
        std::shared_ptr<impl::LogicalDeviceImpl> data_ = nullptr;
        bool operator==(const LogicalDeviceAccessor&) const = default;
        static LogicalDeviceAccessor Create(PhysicalDevice physical, DeviceCreateInfo info = {});
        lifetime::OwnerRef owner_ref() const;
        PhysicalDevice get_physical_device() const;
        DeviceFeature enabled_features() const;
    };

    namespace helper {
        void retain_physical_device_owner(const std::shared_ptr<impl::LogicalDeviceImpl>& logical, const PhysicalDevice& physical);
        Surface checked_surface(const std::shared_ptr<Surface>& surface);
    } // namespace helper

    template<typename C = LogicalDeviceGraphicCreator<VALIDATION>>
    struct LogicalDevice : public LogicalDeviceAccessor {
        LogicalDevice(PhysicalDevice device, VSL_NAMESPACE::Surface surface) {
            C creator;
            data_ = creator.create(device, surface);
            helper::retain_physical_device_owner(data_, device);
        }
        LogicalDevice(PhysicalDevice device, std::shared_ptr<VSL_NAMESPACE::Surface> surface)
            : LogicalDevice(std::move(device), helper::checked_surface(surface)) {}

#ifdef VSL_NAMESPACE_TEST
#endif
    };
} // namespace VSL_NAMESPACE

template<bool validation>
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S> VSL_NAMESPACE::Vulkan<validation>::devices() {
    return VSL_NAMESPACE::PhysicalDevices<S>(*this);
}
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::PhysicalDevices(VulkanAccessor vulkan)
    : _collection(std::move(vulkan)) {}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::~PhysicalDevices() = default;

template<typename S>
std::vector<VSL_NAMESPACE::PhysicalDevice> VSL_NAMESPACE::PhysicalDevices<S>::get() {
    return _collection.get();
}

template<typename S>
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevices<S>::search() {
    if constexpr (std::same_as<S, PhysicalDevicesSercher>) {
        return _collection.select();
    } else {
        auto snapshot = _collection;
        S searcher;
        const auto candidates = std::move(snapshot).get();
        int max_score = -1;
        std::optional<PhysicalDevice> selected;
        for (auto device : candidates) {
            const int score = searcher.benchmark(device);
            if (max_score < score) {
                max_score = score;
                selected = std::move(device);
            }
        }
        if (!selected)
            throw exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices is not suitable.");
        return std::move(*selected);
    }
}
