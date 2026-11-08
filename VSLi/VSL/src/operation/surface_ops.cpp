#include <VSL/operation/surface_ops.hpp>
#include <VSL/exceptions.hpp>
#include <stdexcept>
#include <utility>

namespace VSL_NAMESPACE::operation {
    void initialize_surface(impl::SurfaceImpl& target, impl::VulkanImplAccessor& instance, void* window, const SurfaceNativeCalls& calls) {
        if (target.surface || target.vulkan)
            throw std::logic_error("Surface is already initialized");
        if (!instance.instance || !window)
            throw std::invalid_argument("Surface requires a live Instance and OS window");
        VkSurfaceKHR native = VK_NULL_HANDLE;
        const auto result = calls.create ? calls.create(calls.context, instance.instance, window, &native) : glfwCreateWindowSurface(instance.instance, static_cast<GLFWwindow*>(window), nullptr, &native);
        if (result != VK_SUCCESS || !native) {
            const char* description = nullptr;
            const auto error = glfwGetError(&description);
            throw exceptions::RuntimeException("Surface", "Failed to create GLFW window surface: " + std::string(description ? description : "Unknown GLFW error") +
                                                              " (VkResult " + std::to_string(result) + ", GLFW " + std::to_string(error) + ")");
        }
        target.vulkan = &instance;
        target.native_instance = instance.instance;
        target.window = window;
        target.surface = vk::SurfaceKHR(native);
    }
    void destroy_surface(impl::SurfaceImpl& target, const SurfaceNativeCalls& calls) noexcept {
        const auto native = std::exchange(target.surface, {});
        if (!native || !target.native_instance)
            return;
        if (calls.destroy)
            calls.destroy(calls.context, target.native_instance, native);
        else
            target.native_instance.destroySurfaceKHR(native);
    }
} // namespace VSL_NAMESPACE::operation

VSL_NAMESPACE::impl::SurfaceImpl::~SurfaceImpl() {
    operation::destroy_surface(*this);
}
