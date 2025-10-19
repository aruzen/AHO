#pragma once
#include "../define.h"

#include "VSL/utils/string.h"
#include <vector>
#include <any>

#include "../test.h"
#include "pv.h"

#include <memory>
#include <concepts>

namespace VSL_NAMESPACE {
	struct PhysicalDevice;
	struct _PhysicalDevicesSercher;

	template<typename S = VSL_NAMESPACE::_PhysicalDevicesSercher>
	struct PhysicalDevices;

	static std::vector<const char*> validationExtensions = {
			"VK_EXT_debug_utils"
	};

	static std::vector<const char*> validationLayers = {
			"VK_LAYER_KHRONOS_validation"
	};

	struct VulkanAccessor {
		std::shared_ptr<_impl::Vulkan_impl_accessor> _accessor;
	};

	template<bool Validation = VSL_NAMESPACE::validation>
	struct Vulkan : VulkanAccessor {
	public: 
		bool checkValidationLayerSupport();

		Vulkan(const char* app_name);
		Vulkan(const char* app_name, const std::vector<const char*>& requireExtensions);
		~Vulkan();

		template<typename S = VSL_NAMESPACE::_PhysicalDevicesSercher>
		PhysicalDevices<S> devices();

#ifdef VSL_NAMESPACE_TEST 
		void test();
#endif // VSL_NAMESPACE_TEST 
		std::shared_ptr<_impl::Vulkan_impl<Validation>> _data;
	};
}