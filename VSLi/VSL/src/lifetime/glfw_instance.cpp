#include <VSL/lifetime/glfw_lease.hpp>
#include <VSL/vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    InstanceCreateInfo glfw_instance_info(InstanceCreateInfo info, PresentFencePolicy present_fences) {
        if (present_fences != PresentFencePolicy::LEGACY && present_fences != PresentFencePolicy::REQUIRE_FENCE)
            throw std::invalid_argument("Unknown presentation fence policy");
        if (present_fences == PresentFencePolicy::REQUIRE_FENCE) {
            info.extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
            info.extensions.emplace_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            info.extensions.emplace_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            if (!info.api_version || info.api_version < VK_API_VERSION_1_1)
                info.extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }
        GLFWCallScope scope;
        const auto lease = GLFWLease::Acquire();
        std::uint32_t count = 0;
        const auto* extensions = glfwGetRequiredInstanceExtensions(&count);
        if (count && !extensions)
            throw std::runtime_error("GLFW instance extensions are unavailable");
        for (std::uint32_t i = 0; i < count; ++i) {
            if (!extensions[i])
                throw std::invalid_argument("GLFW instance extension is null");
            info.extensions.emplace_back(extensions[i]);
        }
        if (info.validation) {
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
        return info;
    }
} // namespace VSL_NAMESPACE::lifetime
