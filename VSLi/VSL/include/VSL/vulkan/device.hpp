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

	namespace PhysicalDeviceType {
		class _PhysicalDeviceType : public type<_PhysicalDeviceType> {
		public:
			consteval _PhysicalDeviceType(const char* name_, char index_) : type(name_, index_) {}
		};
		constexpr _PhysicalDeviceType Other = _PhysicalDeviceType("OTHER", 0);
		constexpr _PhysicalDeviceType IntegratedGPU = _PhysicalDeviceType("INTEGRATED_GPU", 1);
		constexpr _PhysicalDeviceType DiscreteGPU = _PhysicalDeviceType("DISCRETE_GPU", 2);
		constexpr _PhysicalDeviceType VirtualGPU = _PhysicalDeviceType("VIRTUAL_GPU", 3);
		constexpr _PhysicalDeviceType CPU = _PhysicalDeviceType("CPU", 4);

		
		_PhysicalDeviceType getType(int a);
	};

	struct _PhysicalDevicesSercher {
		int benchmark(PhysicalDevice device);
	};

	template<bool V>
	struct _LogicalDeviceGraphicCreator {
		std::shared_ptr<_impl::LogicalDevice_impl> create(PhysicalDevice device, Surface surface);
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
#endif // VSL_NAMESPACE_TEST
		std::shared_ptr<_impl::PhysicalDevices_impl> _data = nullptr;
	};

	struct PhysicalDevice {
		PhysicalDevice(std::shared_ptr<_impl::PhysicalDevice_impl> _data);
		~PhysicalDevice();

		uint32_t apiVersion();
		uint32_t driverVersion();
		uint32_t vendorID();
		uint32_t deviceID();
		PhysicalDeviceType::_PhysicalDeviceType type();
		std::string name();
		std::shared_ptr<_impl::PhysicalDevice_impl> _data;
	};

	struct LogicalDeviceAccessor {
		std::shared_ptr<_impl::LogicalDevice_impl> _data = nullptr;
	};

	namespace helper {
		void retainPhysicalDeviceOwner(const std::shared_ptr<_impl::LogicalDevice_impl>& logical, const PhysicalDevice& physical);
	}

	template<typename C = _LogicalDeviceGraphicCreator<validation>>
	struct LogicalDevice : public LogicalDeviceAccessor {
		LogicalDevice(PhysicalDevice device, vsl::Surface surface) {
			C creator;
			_data = creator.create(device, surface);
			helper::retainPhysicalDeviceOwner(_data, device);
		}
		LogicalDevice(PhysicalDevice device, std::shared_ptr<vsl::Surface> surface)
			: LogicalDevice(device, *surface) {}

#ifdef VSL_NAMESPACE_TEST
		// void test();
#endif // VSL_NAMESPACE_TEST
	};
}

template<bool Validation>
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S> VSL_NAMESPACE::Vulkan<Validation>::devices()
{
	return VSL_NAMESPACE::PhysicalDevices<S>(*this);
}
namespace VSL_NAMESPACE::helper {
    std::shared_ptr<_impl::PhysicalDevices_impl> enumeratePhysicalDevices(VulkanAccessor vulkan);
    std::vector<PhysicalDevice> physicalDevices(const std::shared_ptr<_impl::PhysicalDevices_impl> &data);
}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::PhysicalDevices(VulkanAccessor vulkan)
    : _data(helper::enumeratePhysicalDevices(std::move(vulkan))) {}

template<typename S>
VSL_NAMESPACE::PhysicalDevices<S>::~PhysicalDevices() = default;

template<typename S>
std::vector<VSL_NAMESPACE::PhysicalDevice> VSL_NAMESPACE::PhysicalDevices<S>::get() {
    return helper::physicalDevices(_data);
}

template<typename S>
VSL_NAMESPACE::PhysicalDevice VSL_NAMESPACE::PhysicalDevices<S>::search() {
    S searcher;
    int max_score = -1;
    std::shared_ptr<_impl::PhysicalDevice_impl> selected;
    for (auto device : get()) {
        const int score = searcher.benchmark(device);
        if (max_score < score) {
            max_score = score;
            selected = device._data;
        }
    }
    if (!selected)
        throw exceptions::RuntimeException("PhysicalDevices", "PhysicalDevices is not suitable.");
    return PhysicalDevice(selected);
}
