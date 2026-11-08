#include <VSL/vulkan/vulkan.hpp>
#include <VSL/lifetime/instance_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    VulkanAccessor VulkanAccessor::Create(InstanceCreateInfo info) {
        return {lifetime::make_heap_instance(std::move(info))};
    }
    lifetime::OwnerRef VulkanAccessor::owner_ref() const {
        if (!accessor || !accessor.use_count() || !accessor->owner_node || !accessor->owner_node->matches(accessor))
            throw std::invalid_argument("Instance requires its canonical owner");
        return accessor->owner_node->ref(accessor);
    }
    template<bool validation>
    bool Vulkan<validation>::check_validation_layer_support() {
        std::vector<std::string> layers;
        layers.reserve(VALIDATION_LAYERS.size());
        for (const auto* layer : VALIDATION_LAYERS) {
            if (!layer)
                return false;
            layers.emplace_back(layer);
        }
        return operation::instance_layers_supported(layers);
    }
    template<bool validation>
    Vulkan<validation>::Vulkan(const char* app_name, const std::vector<const char*>& require_extensions) {
        if (!app_name)
            throw std::invalid_argument("Instance application name is null");
        InstanceCreateInfo info;
        info.application_name = app_name;
        info.validation = validation;
        info.debug_printf = validation;
        std::uint32_t count = 0;
        const auto* glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
        if (count && !glfw_extensions)
            throw std::runtime_error("GLFW instance extensions are unavailable");
        for (std::uint32_t i = 0; i < count; ++i) {
            if (!glfw_extensions[i])
                throw std::invalid_argument("GLFW instance extension is null");
            info.extensions.emplace_back(glfw_extensions[i]);
        }
        for (const auto* extension : require_extensions) {
            if (!extension)
                throw std::invalid_argument("Instance extension is null");
            info.extensions.emplace_back(extension);
        }
        if constexpr (validation) {
            for (const auto* extension : VALIDATION_EXTENSIONS) {
                if (!extension)
                    throw std::invalid_argument("Validation extension is null");
                info.extensions.emplace_back(extension);
            }
            for (const auto* layer : VALIDATION_LAYERS) {
                if (!layer)
                    throw std::invalid_argument("Validation layer is null");
                info.layers.emplace_back(layer);
            }
        }
        accessor = lifetime::make_heap_instance(std::move(info));
        data_ = accessor;
    }
    template<bool validation>
    Vulkan<validation>::~Vulkan() = default;

#ifdef VSL_NAMESPACE_TEST
    template<bool validation>
    void Vulkan<validation>::test() {
        for (const auto device : data_->instance.enumeratePhysicalDevices())
            logging(device.getProperties().deviceName);
    }
#endif
    template struct Vulkan<true>;
    template struct Vulkan<false>;
} // namespace VSL_NAMESPACE
