#include <VSL/define.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/surface.hpp>

#ifdef _MSC_VER
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE_CC__)
#define GLFW_EXPOSE_NATIVE_MACOS
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <memory>

#include <VSL/window_plugin.hpp>
#include <VSL/exceptions.hpp>

VSL_NAMESPACE::Surface::Surface(VSL_NAMESPACE::PureWindow* window, VSL_NAMESPACE::VulkanAccessor instance) {
    if (!window || !window->data_ || !window->data_->window_handle)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Surface", "A live window is required");
    if (!instance.accessor || !instance.accessor->instance)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Surface", "A Vulkan instance is required");
    data_ = std::make_shared<VSL_NAMESPACE::impl::SurfaceImpl>();
    data_->vulkan = instance.accessor;
    data_->window = window->data_->window_handle;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const auto result = glfwCreateWindowSurface(data_->vulkan->instance, (GLFWwindow*)data_->window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        const char* description = nullptr;
        const int error = glfwGetError(&description);
        throw VSL_NAMESPACE::exceptions::RuntimeException(
            "Surface", "Failed to create GLFW window surface: " + std::string(description ? description : "Unknown GLFW error") +
                           " (VkResult " + std::to_string(result) + ", GLFW " + std::to_string(error) + ")");
    }
    data_->surface = vk::SurfaceKHR(surface);
    /*
	vk::Win32SurfaceCreateInfoKHR createInfo;
	createInfo.hwnd = glfwGetWin32Window((GLFWwindow*)data->window_handle);
	createInfo.hinstance = GetModuleHandle(nullptr);

	data_->surface = data_->instance.createWin32SurfaceKHR(createInfo);
	*/
}

VSL_NAMESPACE::impl::SurfaceImpl::~SurfaceImpl() {
    if (vulkan && surface)
        vulkan->instance.destroySurfaceKHR(surface);
}
