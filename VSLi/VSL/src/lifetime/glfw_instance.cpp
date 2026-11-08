#include <VSL/lifetime/glfw_lease.hpp>
#include <VSL/vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime {
    InstanceCreateInfo glfw_instance_info(InstanceCreateInfo info) {
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
