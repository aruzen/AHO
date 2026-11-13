#include <VSL/vulkan/vulkan.hpp>
#include <VSL/lifetime/startup_access.hpp>
#include <VSL/lifetime/instance_storage.hpp>
#include <VSL/lifetime/dependency_node.hpp>
#include <VSL/lifetime/glfw_lease.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE {
    using lifetime::detail::StartupAccess;
    VulkanAccessor VulkanAccessor::Create(InstanceCreateInfo info) {
        return StartupAccess::FromShared(lifetime::make_heap_instance(std::move(info)));
    }
    lifetime::OwnerRef VulkanAccessor::owner_ref() const {
        StartupAccess::RequireCanonical(_owner);
        return _owner;
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
        for (const auto* extension : require_extensions) {
            if (!extension)
                throw std::invalid_argument("Instance extension is null");
            info.extensions.emplace_back(extension);
        }
        _value = StartupAccess::FromShared(lifetime::make_heap_instance(lifetime::glfw_instance_info(std::move(info))));
    }
    template<bool validation>
    Vulkan<validation>::Vulkan(VulkanAccessor instance) {
        StartupAccess::RequireCanonical(StartupAccess::Owned(instance));
        if (!StartupAccess::Owned(instance)->instance)
            throw std::invalid_argument("Vulkan facade requires a live canonical Instance");
        _value = std::move(instance);
    }
    template<bool validation>
    Vulkan<validation>::~Vulkan() = default;

#ifdef VSL_NAMESPACE_TEST
    template<bool validation>
    void Vulkan<validation>::test() {
        const auto owner = StartupAccess::Owned(*this);
        for (const auto device : owner->instance.enumeratePhysicalDevices())
            logging(device.getProperties().deviceName);
    }
#endif
    template struct Vulkan<true>;
    template struct Vulkan<false>;
} // namespace VSL_NAMESPACE
