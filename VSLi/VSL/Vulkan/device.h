#pragma once
#include "../define.h"
#include "Vulkan.h"

#include "../Type.h"
#include "../concepts.h"

#include <any>

namespace VSL_NAMESPACE {
	struct Surface;

	namespace PhysicalDeviceType {
		class _PhysicalDeviceType : public Type<_PhysicalDeviceType> {
		public:
			consteval _PhysicalDeviceType(const char* name_, char index_) :Type(name_, index_) {}
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
		using BenchMarcker = typename S;

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

	template<typename C = _LogicalDeviceGraphicCreator<validation>>
	struct LogicalDevice : public LogicalDeviceAccessor {
		LogicalDevice(PhysicalDevice device, vsl::Surface surface);
		LogicalDevice(PhysicalDevice device, std::shared_ptr<vsl::Surface> surface);

#ifdef VSL_NAMESPACE_TEST
		// void test();
#endif // VSL_NAMESPACE_TEST
	};
}

template<bool Validation>
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S> VSL_NAMESPACE::Vulkan<Validation>::devices()
{
	return VSL_NAMESPACE::PhysicalDevices<S, Validation>(*this);
}