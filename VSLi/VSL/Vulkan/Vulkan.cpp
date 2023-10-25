#include "pch.h"
#include "../define.h"
#include "Vulkan.h"

#include "_pimpls.h"

#include "../exceptions.h"
#include "../debug.h"

template<bool Validation>
bool VSL_NAMESPACE::Vulkan<Validation>::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	using namespace std;

	VSL_NAMESPACE::loggingln("validation : "s + pCallbackData->pMessage);

	return VK_FALSE;
}

void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
	createInfo.pfnUserCallback = debugCallback;
}

vk::Result CreateDebugUtilsMessengerEXT(vk::Instance instance, const vk::DebugUtilsMessengerCreateInfoEXT& pCreateInfo, const vk::AllocationCallbacks* pAllocator, vk::DebugUtilsMessengerEXT& pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		VkDebugUtilsMessengerEXT tmpDebugMessenger;
		auto result = (vk::Result)func(instance, &((const VkDebugUtilsMessengerCreateInfoEXT&)pCreateInfo), &((const VkAllocationCallbacks&)*pAllocator) , &tmpDebugMessenger);
		pDebugMessenger = tmpDebugMessenger;
		return result;
	}
	else {
		return vk::Result::eErrorExtensionNotPresent;
	}
}

template<bool V>
void setupDebugMessenger(std::shared_ptr<VSL_NAMESPACE::_impl::Vulkan_impl<V>> data) {
	vk::DebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(data->instance, createInfo, nullptr, data->debugMessenger) != vk::Result::eSuccess) {
		throw VSL_NAMESPACE::exceptions::RuntimeException("failed to set up debug messenger!");
	}
}

template<bool Validation>
VSL_NAMESPACE::Vulkan<Validation>::Vulkan(const char* app_name, const std::vector<const char*>& requireExtensions)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::Vulkan_impl<Validation>>(new VSL_NAMESPACE::_impl::Vulkan_impl<Validation>);

	if constexpr (Validation) {
		checkValidationLayerSupport();
	}

	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName = app_name;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "VSL_NAMESPACE Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	vk::InstanceCreateInfo createInfo;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if constexpr (Validation) {
		for (const auto& e : validationLayers)
			extensions.push_back(e);
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	}

	for (auto e : requireExtensions)
		extensions.push_back(e);

	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (!(_data->instance = vk::createInstance(createInfo))) {
		throw VSL_NAMESPACE::exceptions::RuntimeException("VulkanInstance", "create failed.");
	}
	
	if constexpr (Validation) {
		std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();
		VSL_NAMESPACE::loggingln("available extensions:\n");

		for (const auto& extension : extensions) {
			VSL_NAMESPACE::loggingln(extension.extensionName);
		}

		setupDebugMessenger(_data);
	} else {
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
		FreeConsole();
	}
}

template<bool Validation>
VSL_NAMESPACE::Vulkan<Validation>::Vulkan(const char* app_name)
{
	_data = std::shared_ptr<VSL_NAMESPACE::_impl::Vulkan_impl<Validation>>(new VSL_NAMESPACE::_impl::Vulkan_impl<Validation>);

	if constexpr (Validation) {
		checkValidationLayerSupport();
	}
	
	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName = app_name;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "VSL_NAMESPACE Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	vk::InstanceCreateInfo createInfo;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if constexpr (Validation) {
		for (const auto& e : validationLayers)
			extensions.push_back(e);
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (!(_data->instance = vk::createInstance(createInfo))) {
		throw VSL_NAMESPACE::exceptions::RuntimeException("VulkanInstance", "create failed.");
	}

	if constexpr (Validation) {
		auto extensions = vk::enumerateInstanceExtensionProperties();
		VSL_NAMESPACE::loggingln("available extensions:\n");

		for (const auto& extension : extensions) {
			VSL_NAMESPACE::loggingln(extension.extensionName);
		}

		setupDebugMessenger(_data);
	} else {
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
		FreeConsole();
	}
}

void vsl::_impl::helper::DestroyDebugUtilsMessengerEXT(vk::Instance& instance, vk::DebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

template<bool Validation>
VSL_NAMESPACE::Vulkan<Validation>::~Vulkan()
{
	
}

#ifdef VSL_NAMESPACE_TEST
template<bool Validation>
void VSL_NAMESPACE::Vulkan<Validation>::test()
{
 	auto devices = _data->instance.enumeratePhysicalDevices();
	for (auto d : devices) {
		auto props = d.getProperties();
		VSL_NAMESPACE::logging(props.deviceName);
	}
}
#endif // VSL_NAMESPACE_TEST

template<bool V>
VSL_NAMESPACE::_impl::Vulkan_impl<V>::~Vulkan_impl()
{
	if constexpr (V) {
		helper::DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	instance.destroy();
}

template struct VSL_NAMESPACE::_impl::Vulkan_impl<true>;
template struct VSL_NAMESPACE::Vulkan<true>;
template struct VSL_NAMESPACE::_impl::Vulkan_impl<false>;
template struct VSL_NAMESPACE::Vulkan<false>;