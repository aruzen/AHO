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
    if (!window || !window->_data || !window->_data->window_handle)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Surface", "A live window is required");
    if (!instance._accessor || !instance._accessor->instance)
        throw VSL_NAMESPACE::exceptions::RuntimeException("Surface", "A Vulkan instance is required");
    _data = std::make_shared<VSL_NAMESPACE::_impl::Surface_impl>();
    _data->vulkan = instance._accessor;
    _data->window = window->_data->window_handle;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const auto result = glfwCreateWindowSurface(_data->vulkan->instance, (GLFWwindow*)_data->window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        const char* description = nullptr;
        const int error = glfwGetError(&description);
        throw VSL_NAMESPACE::exceptions::RuntimeException(
            "Surface", "Failed to create GLFW window surface: " + std::string(description ? description : "Unknown GLFW error") +
                           " (VkResult " + std::to_string(result) + ", GLFW " + std::to_string(error) + ")");
    }
    _data->surface = vk::SurfaceKHR(surface);
    /*
	vk::Win32SurfaceCreateInfoKHR createInfo;
	createInfo.hwnd = glfwGetWin32Window((GLFWwindow*)data->window_handle);
	createInfo.hinstance = GetModuleHandle(nullptr);

	_data->surface = _data->instance.createWin32SurfaceKHR(createInfo);
	*/
}

VSL_NAMESPACE::_impl::Surface_impl::~Surface_impl() {
    if (vulkan && surface)
        vulkan->instance.destroySurfaceKHR(surface);
}
