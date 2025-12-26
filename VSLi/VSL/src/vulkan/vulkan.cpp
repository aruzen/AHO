#include <VSL/define.hpp>
#include <VSL/vulkan/vulkan.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>
#include <VSL/utils/string.hpp>

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

static VkBool32 VKAPI_PTR debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	using namespace std;

	string message(pCallbackData->pMessage);
	if (message.substr(0, 20).contains("Error"))
		VSL_NAMESPACE::loggingln("\033[31mvalidation\033[0m : "s + pCallbackData->pMessage);
	else
		VSL_NAMESPACE::loggingln("\033[32mvalidation\033[0m : "s + pCallbackData->pMessage);

	return VK_FALSE;
}

void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.pfnUserCallback = (vk::PFN_DebugUtilsMessengerCallbackEXT)debugCallback;
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
    _accessor = std::dynamic_pointer_cast<_impl::Vulkan_impl_accessor>(_data);

    if constexpr (Validation) {
        checkValidationLayerSupport();

        std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();
        VSL_NAMESPACE::loggingln("available extensions:");
        for (const auto &extension: extensions)
            VSL_NAMESPACE::loggingln(extension.extensionName);
        std::vector<vk::LayerProperties> layers = vk::enumerateInstanceLayerProperties();
        VSL_NAMESPACE::loggingln("available layers:");
        for (const auto& layer : layers)
            VSL_NAMESPACE::loggingln(layer.layerName);
    }

    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = app_name;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VSLEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vk::InstanceCreateInfo createInfo;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    for (auto e : requireExtensions)
        extensions.push_back(e);

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    vk::ValidationFeaturesEXT validation_features;
    std::vector<vk::ValidationFeatureEnableEXT>  validation_feature_enables = {
            vk::ValidationFeatureEnableEXT::eDebugPrintf
    };
    if constexpr (Validation) {
        for (const auto& e : validationExtensions)
            extensions.push_back(e);

        createInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;

        populateDebugMessengerCreateInfo(debugCreateInfo);
        validation_features.pNext = &debugCreateInfo;

        validation_features.enabledValidationFeatureCount = 1;
        validation_features.pEnabledValidationFeatures    = validation_feature_enables.data();
        createInfo.pNext = &validation_features;

        // validation layer
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if constexpr (Validation) {
        loggingln("enabled extensions:");
        for (const auto& e : extensions)
            VSL_NAMESPACE::loggingln(e);
    }

    if (!(_data->instance = vk::createInstance(createInfo))) {
        throw VSL_NAMESPACE::exceptions::RuntimeException("VulkanInstance", "create failed.");
    }

    if constexpr (Validation) {
        setupDebugMessenger(_data);
    } else {
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
#ifdef _MSC_VER
        FreeConsole();
#endif
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