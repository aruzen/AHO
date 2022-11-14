#pragma once
#include "../define.h"
#include "Vulkan.h"

#include "../Type.h"
#include "../concepts.h"

#include <any>

namespace VSL_NAMESPACE {
	template< bool Validation = vsl::validation>
	struct Surface;

	namespace PhysicalDeviceType {
		class _PhysicalDeviceType : public VSL_NAMESPACE::Type<_PhysicalDeviceType> {
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
		int benchmark(VSL_NAMESPACE::PhysicalDevice device);
	};

	template<bool V>
	struct _LogicalDeviceGraphicCreator {
		std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> create(VSL_NAMESPACE::PhysicalDevice device, VSL_NAMESPACE::Surface<V> surface);
	};

	template<typename S, bool Validation>
	struct PhysicalDevices {
		using BenchMarcker = typename S;

		PhysicalDevices(const VSL_NAMESPACE::Vulkan<Validation>& vulkan);
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
		VSL_NAMESPACE::PhysicalDeviceType::_PhysicalDeviceType type();
		std::string name();
		std::shared_ptr<_impl::PhysicalDevice_impl> _data;
	};

	struct LogicalDeviceAcsessor {
		std::shared_ptr<VSL_NAMESPACE::_impl::LogicalDevice_impl> _data = nullptr;
	};

	template<typename C = VSL_NAMESPACE::_LogicalDeviceGraphicCreator<VSL_NAMESPACE::validation>, bool V = VSL_NAMESPACE::validation>
	struct LogicalDevice : public LogicalDeviceAcsessor {
		LogicalDevice(VSL_NAMESPACE::PhysicalDevice device, vsl::Surface<V> surface);
		LogicalDevice(VSL_NAMESPACE::PhysicalDevice device, std::shared_ptr<vsl::Surface<V>> surface);

#ifdef VSL_NAMESPACE_TEST
		void test();
#endif // VSL_NAMESPACE_TEST
	};
}

template<bool Validation>
template<typename S>
VSL_NAMESPACE::PhysicalDevices<S, Validation> VSL_NAMESPACE::Vulkan<Validation>::devices()
{
	return VSL_NAMESPACE::PhysicalDevices<S, Validation>(*this);
}

template struct VSL_NAMESPACE::Vulkan<true>;
template struct VSL_NAMESPACE::PhysicalDevices<VSL_NAMESPACE::_PhysicalDevicesSercher, true>;