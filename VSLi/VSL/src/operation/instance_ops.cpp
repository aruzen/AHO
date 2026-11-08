#include <VSL/operation/instance_ops.hpp>
#include <VSL/utils/string.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::operation {
    namespace {
        void validate_text(const std::string& value, bool allow_empty);
        void normalize(std::vector<std::string>& names);
        void append_name(std::vector<std::string>& names, const char* name);
        std::vector<const char*> native_names(const std::vector<std::string>& names);
        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* data, void*) noexcept;

        void validate_text(const std::string& value, bool allow_empty) {
            if ((!allow_empty && value.empty()) || value.find('\0') != std::string::npos)
                throw std::invalid_argument("Instance names must be valid C strings");
        }
        void normalize(std::vector<std::string>& names) {
            if (names.size() > std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("Instance name count exceeds uint32");
            size_t count = 0;
            for (size_t i = 0; i < names.size(); ++i) {
                validate_text(names[i], false);
                if (std::find(names.begin(), names.begin() + count, names[i]) == names.begin() + count) {
                    if (i != count)
                        names[count] = std::move(names[i]);
                    ++count;
                }
            }
            names.resize(count);
        }
        void append_name(std::vector<std::string>& names, const char* name) {
            if (std::ranges::find(names, name) == names.end())
                names.emplace_back(name);
        }
        std::vector<const char*> native_names(const std::vector<std::string>& names) {
            if (names.size() > std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("Instance name count exceeds uint32");
            std::vector<const char*> result;
            result.reserve(names.size());
            for (const auto& name : names)
                result.push_back(name.c_str());
            return result;
        }
        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* data, void*) noexcept {
            try {
                if (data && data->pMessage)
                    loggingln(std::string(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "\033[31mvalidation\033[0m : " : "\033[32mvalidation\033[0m : ") + data->pMessage);
            } catch (...) {
                // C callback境界へ例外を出さない。
            }
            return VK_FALSE;
        }
    } // namespace
    bool instance_layers_supported(std::span<const std::string> layers) {
        if (layers.empty())
            return true;
        const auto available = vk::enumerateInstanceLayerProperties();
        return std::ranges::all_of(layers, [&](const auto& name) {
            return !name.empty() && name.find('\0') == std::string::npos && std::ranges::any_of(available, [&](const auto& layer) { return name == layer.layerName.data(); });
        });
    }
    void initialize_instance(impl::VulkanImplAccessor& instance, InstanceCreateInfo info, const InstanceNativeCalls& calls) {
        if (instance.instance || instance.debug_messenger)
            throw std::logic_error("Instance is already initialized");
        validate_text(info.application_name, true);
        validate_text(info.engine_name, true);
        normalize(info.extensions);
        normalize(info.layers);
        const bool surface = std::ranges::find(info.extensions, VK_KHR_SURFACE_EXTENSION_NAME) != info.extensions.end();
        const bool capabilities2 = std::ranges::find(info.extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) != info.extensions.end();
        const bool maintenance1 = std::ranges::find(info.extensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) != info.extensions.end();
        if ((capabilities2 && !surface) || (maintenance1 && (!surface || !capabilities2)))
            throw std::invalid_argument("Surface maintenance requires KHR_surface and KHR_get_surface_capabilities2");
        if (info.debug_printf && !info.validation)
            throw std::invalid_argument("Debug printf requires validation");
        if (info.validation) {
            append_name(info.layers, "VK_LAYER_KHRONOS_validation");
            append_name(info.extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        if (info.debug_printf)
            append_name(info.extensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        if (!instance_layers_supported(info.layers))
            throw std::invalid_argument("A requested instance layer is unavailable");
        auto available = vk::enumerateInstanceExtensionProperties();
        for (const auto& layer : info.layers) {
            const auto extra = vk::enumerateInstanceExtensionProperties(layer);
            available.insert(available.end(), extra.begin(), extra.end());
        }
        for (const auto& name : info.extensions)
            if (!std::ranges::any_of(available, [&](const auto& extension) { return name == extension.extensionName.data(); }))
                throw std::invalid_argument("Instance extension is unavailable: " + name);
        const auto extensions = native_names(info.extensions), layers = native_names(info.layers);
        const VkApplicationInfo app{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = info.application_name.c_str(), .applicationVersion = info.application_version, .pEngineName = info.engine_name.c_str(), .engineVersion = info.engine_version, .apiVersion = info.api_version};
        const VkDebugUtilsMessengerCreateInfoEXT debug{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, .pfnUserCallback = debug_callback};
        const VkValidationFeatureEnableEXT feature = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
        const VkValidationFeaturesEXT validation{.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, .pNext = &debug, .enabledValidationFeatureCount = 1, .pEnabledValidationFeatures = &feature};
        const VkInstanceCreateInfo native{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = info.debug_printf ? static_cast<const void*>(&validation) : info.validation ? &debug
                                                                                                                                                                                : nullptr,
                                          .flags = std::ranges::find(info.extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) != info.extensions.end() ? VkInstanceCreateFlags{VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR} : VkInstanceCreateFlags{},
                                          .pApplicationInfo = &app,
                                          .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
                                          .ppEnabledLayerNames = layers.data(),
                                          .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
                                          .ppEnabledExtensionNames = extensions.data()};
        impl::VulkanImplAccessor pending;
        try {
            VkInstance handle = VK_NULL_HANDLE;
            const auto result = calls.create_instance ? calls.create_instance(calls.context, &native, &handle)
                                                      : vkCreateInstance(&native, nullptr, &handle);
            if (result != VK_SUCCESS)
                vk::detail::throwResultException(static_cast<vk::Result>(result), "Instance creation failed");
            if (!handle)
                throw std::runtime_error("Instance creation returned an empty handle");
            pending.instance = handle;
            if (info.validation) {
                const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(handle, "vkCreateDebugUtilsMessengerEXT"));
                pending.destroy_debug_messenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(handle, "vkDestroyDebugUtilsMessengerEXT"));
                if (!create || !pending.destroy_debug_messenger)
                    throw std::runtime_error("Instance debug messenger entry points are unavailable");
                VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
                const auto messenger_result = calls.create_messenger ? calls.create_messenger(calls.context, handle, &debug, &messenger)
                                                                     : create(handle, &debug, nullptr, &messenger);
                if (messenger_result != VK_SUCCESS)
                    vk::detail::throwResultException(static_cast<vk::Result>(messenger_result), "Instance debug messenger creation failed");
                if (!messenger)
                    throw std::runtime_error("Debug messenger creation returned an empty handle");
                pending.debug_messenger = messenger;
            }
        } catch (...) {
            destroy_instance(pending, calls);
            throw;
        }
        instance.instance = std::exchange(pending.instance, nullptr);
        instance.debug_messenger = std::exchange(pending.debug_messenger, nullptr);
        instance.destroy_debug_messenger = std::exchange(pending.destroy_debug_messenger, nullptr);
        instance.api_version = info.api_version ? info.api_version : VK_API_VERSION_1_0;
        instance.physical_device_properties2 = std::ranges::find(info.extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) != info.extensions.end();
        instance.surface_capabilities2 = capabilities2;
        instance.surface_maintenance1 = maintenance1;
    }
    void destroy_instance(impl::VulkanImplAccessor& instance, const InstanceNativeCalls& calls) noexcept {
        if (instance.debug_messenger) {
            const auto destroy = instance.destroy_debug_messenger ? instance.destroy_debug_messenger : !instance.owner_node ? reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance.instance, "vkDestroyDebugUtilsMessengerEXT"))
                                                                                                                            : nullptr;
            if (calls.destroy_messenger)
                calls.destroy_messenger(calls.context, instance.instance, instance.debug_messenger);
            else if (destroy)
                destroy(instance.instance, instance.debug_messenger, nullptr);
            instance.debug_messenger = nullptr;
        }
        instance.destroy_debug_messenger = nullptr;
        if (instance.instance) {
            if (calls.destroy_instance)
                calls.destroy_instance(calls.context, instance.instance);
            else
                vkDestroyInstance(instance.instance, nullptr);
            instance.instance = nullptr;
        }
        instance.surface_capabilities2 = false;
        instance.surface_maintenance1 = false;
    }
} // namespace VSL_NAMESPACE::operation
