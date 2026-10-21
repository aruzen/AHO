#include <VSL/define.hpp>
#include <VSL/vulkan/vulkan.hpp>

#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/exceptions.hpp>
#include <VSL/utils/string.hpp>

template<bool validation>
bool VSL_NAMESPACE::Vulkan<validation>::check_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : VALIDATION_LAYERS) {
        bool layer_found = false;

        for (const auto& layer_properties : available_layers) {
            if (strcmp(layer_name, layer_properties.layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

static VkBool32 VKAPI_PTR debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data) {

    using namespace std;

    string message(p_callback_data->pMessage);
    if (message.substr(0, 20).contains("Error"))
        VSL_NAMESPACE::loggingln("\033[31mvalidation\033[0m : "s + p_callback_data->pMessage);
    else
        VSL_NAMESPACE::loggingln("\033[32mvalidation\033[0m : "s + p_callback_data->pMessage);

    return VK_FALSE;
}

void populate_debug_messenger_create_info(vk::DebugUtilsMessengerCreateInfoEXT& create_info) {
    create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                 vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                 vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                 vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    create_info.pfnUserCallback = (vk::PFN_DebugUtilsMessengerCallbackEXT)debug_callback;
}

vk::Result create_debug_utils_messenger_ext(vk::Instance instance, const vk::DebugUtilsMessengerCreateInfoEXT& p_create_info, const vk::AllocationCallbacks* p_allocator, vk::DebugUtilsMessengerEXT& p_debug_messenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        VkDebugUtilsMessengerEXT tmp_debug_messenger;
        auto result = (vk::Result)func(instance, &((const VkDebugUtilsMessengerCreateInfoEXT&)p_create_info), reinterpret_cast<const VkAllocationCallbacks*>(p_allocator), &tmp_debug_messenger);
        p_debug_messenger = tmp_debug_messenger;
        return result;
    } else {
        return vk::Result::eErrorExtensionNotPresent;
    }
}

template<bool v>
void setup_debug_messenger(std::shared_ptr<VSL_NAMESPACE::impl::VulkanImpl<v>> data) {
    vk::DebugUtilsMessengerCreateInfoEXT create_info;
    populate_debug_messenger_create_info(create_info);

    if (create_debug_utils_messenger_ext(data->instance, create_info, nullptr, data->debug_messenger) != vk::Result::eSuccess) {
        throw VSL_NAMESPACE::exceptions::RuntimeException("failed to set up debug messenger!");
    }
}

template<bool validation>
VSL_NAMESPACE::Vulkan<validation>::Vulkan(const char* app_name, const std::vector<const char*>& require_extensions) {
    data_ = std::shared_ptr<VSL_NAMESPACE::impl::VulkanImpl<validation>>(new VSL_NAMESPACE::impl::VulkanImpl<validation>);
    accessor = std::dynamic_pointer_cast<impl::VulkanImplAccessor>(data_);

    if constexpr (validation) {
        check_validation_layer_support();

        std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();
        VSL_NAMESPACE::loggingln("available extensions:");
        for (const auto& extension : extensions)
            VSL_NAMESPACE::loggingln(extension.extensionName);
        std::vector<vk::LayerProperties> layers = vk::enumerateInstanceLayerProperties();
        VSL_NAMESPACE::loggingln("available layers:");
        for (const auto& layer : layers)
            VSL_NAMESPACE::loggingln(layer.layerName);
    }

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "VSLEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    vk::InstanceCreateInfo create_info;
    create_info.pApplicationInfo = &app_info;
    for (auto e : require_extensions)
        if (std::string(e) == "VK_KHR_portability_enumeration")
            create_info.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
    for (auto e : require_extensions)
        extensions.push_back(e);

    vk::DebugUtilsMessengerCreateInfoEXT debug_create_info;
    vk::ValidationFeaturesEXT validation_features;
    std::vector<vk::ValidationFeatureEnableEXT> validation_feature_enables = {
        vk::ValidationFeatureEnableEXT::eDebugPrintf,
        vk::ValidationFeatureEnableEXT::eBestPractices,
    };
    if constexpr (validation) {
        for (const auto& e : VALIDATION_EXTENSIONS)
            extensions.push_back(e);
        populate_debug_messenger_create_info(debug_create_info);
        validation_features.pNext = &debug_create_info;

        validation_features.enabledValidationFeatureCount = 1;
        validation_features.pEnabledValidationFeatures = validation_feature_enables.data();
        create_info.pNext = &validation_features;

        // validation layer
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if constexpr (validation) {
        loggingln("enabled extensions:");
        for (const auto& e : extensions)
            VSL_NAMESPACE::loggingln(e);
    }

    if (!(data_->instance = vk::createInstance(create_info))) {
        throw VSL_NAMESPACE::exceptions::RuntimeException("VulkanInstance", "create failed.");
    }

    if constexpr (validation) {
        setup_debug_messenger(data_);
    } else {
        // Release等で検証を無効にする運用では、一般ユーザーに不要なコンソール画面（黒いウィンドウ）を残さない意図の処理。
        // 標準入出力を閉じ、MSVCではFreeConsole()でコンソールから切り離す。
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
#ifdef _MSC_VER
        FreeConsole();
#endif
    }
}

void VSL_NAMESPACE::impl::helper::destroy_debug_utils_messenger_ext(vk::Instance& instance, vk::DebugUtilsMessengerEXT& debug_messenger, const VkAllocationCallbacks* p_allocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debug_messenger, p_allocator);
    }
}

template<bool validation>
VSL_NAMESPACE::Vulkan<validation>::~Vulkan() {
}

#ifdef VSL_NAMESPACE_TEST
template<bool validation>
void VSL_NAMESPACE::Vulkan<validation>::test() {
    auto devices = data_->instance.enumeratePhysicalDevices();
    for (auto d : devices) {
        auto props = d.getProperties();
        VSL_NAMESPACE::logging(props.deviceName);
    }
}
#endif // VSL_NAMESPACE_TEST

template<bool v>
VSL_NAMESPACE::impl::VulkanImpl<v>::~VulkanImpl() {
    if constexpr (v) {
        helper::destroy_debug_utils_messenger_ext(instance, debug_messenger, nullptr);
    }
    instance.destroy();
}

template struct VSL_NAMESPACE::impl::VulkanImpl<true>;
template struct VSL_NAMESPACE::Vulkan<true>;
template struct VSL_NAMESPACE::impl::VulkanImpl<false>;
template struct VSL_NAMESPACE::Vulkan<false>;
