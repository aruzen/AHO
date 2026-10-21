#pragma once
#include "../define.hpp"
#include "vulkan.hpp"
#include "surface.hpp"

#include "../type.hpp"
#include "../concepts.hpp"

#include <any>
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

    template<typename S>
    struct PhysicalDevices {
        using BenchMarcker = S;

        PhysicalDevices(VulkanAccessor vulkan);
        ~PhysicalDevices();

        std::vector<PhysicalDevice> get();
        PhysicalDevice search();
#ifdef VSL_NAMESPACE_TEST
        void test();
#endif
        std::shared_ptr<impl::PhysicalDevicesImpl> data_ = nullptr;
    };

    struct PhysicalDevice {
        PhysicalDevice(std::shared_ptr<impl::PhysicalDeviceImpl> data_);
        ~PhysicalDevice();

        uint32_t api_version();
        uint32_t driver_version();
        uint32_t vendor_id();
        uint32_t device_id();
        physical_device_type::PhysicalDeviceType type();
        std::string name();
        std::shared_ptr<impl::PhysicalDeviceImpl> data_;
    };

    struct LogicalDeviceAccessor {
        std::shared_ptr<impl::LogicalDeviceImpl> data_ = nullptr;
        bool operator==(const LogicalDeviceAccessor&) const = default;
    };

    namespace helper {
        void retain_physical_device_owner(const std::shared_ptr<impl::LogicalDeviceImpl>& logical, const PhysicalDevice& physical);
    }

    template<typename C = LogicalDeviceGraphicCreator<VALIDATION>>
    struct LogicalDevice : public LogicalDeviceAccessor {
        LogicalDevice(PhysicalDevice device, VSL_NAMESPACE::Surface surface) {
            C creator;
            data_ = creator.create(device, surface);
            helper::retain_physical_device_owner(data_, device);
        }
        LogicalDevice(PhysicalDevice device, std::shared_ptr<VSL_NAMESPACE::Surface> surface)
            : LogicalDevice(device, *surface) {}

#ifdef VSL_NAMESPACE_TEST
#endif
    };
} // namespace VSL_NAMESPACE

template<bool validation>
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S> VSL_NAMESPACE::Vulkan<validation>::devices() {
    return VSL_NAMESPACE::PhysicalDevices<S>(*this);
}
namespace VSL_NAMESPACE::helper {
    std::shared_ptr<impl::PhysicalDevicesImpl> enumerate_physical_devices(VulkanAccessor vulkan);
    std::vector<PhysicalDevice> physical_devices(const std::shared_ptr<impl::PhysicalDevicesImpl>& data);
} // namespace VSL_NAMESPACE::helper

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::PhysicalDevices(VulkanAccessor vulkan)
    : data_(helper::enumerate_physical_devices(std::move(vulkan))) {}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::~PhysicalDevices() = default;

template<typename S>
std::vector<VSL_NAMESPACE::PhysicalDevice> VSL_NAMESPACE::PhysicalDevices<S>::get() {
    return helper::physical_devices(data_);
}

template<typename S>
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevices<S>::search() {
    S searcher;
    int max_score = -1;
    std::shared_ptr<impl::PhysicalDeviceImpl> selected;
    for (auto device : get()) {
        const int score = searcher.benchmark(device);
        if (max_score < score) {
            max_score = score;
            selected = device.data_;
        }
    }
    if (!selected)
        throw exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices is not suitable.");
    return PhysicalDevice(selected);
}
