#include <VSL/define.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <VSL/vulkan/surface.hpp>
#include <VSL/exceptions.hpp>

#ifdef _MSC_VER
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE_CC__)
#define GLFW_EXPOSE_NATIVE_MACOS
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <memory>
#include <source_location>

#include <VSL/window_plugin.hpp>

VSL_NAMESPACE::Surface::Surface(VSL_NAMESPACE::PureWindow* window, VSL_NAMESPACE::VulkanAccessor instance)
{
    if (window == nullptr || window->_data == nullptr || window->_data->window_handle == nullptr) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Surface",
                "Surface creation requires a valid window.",
                std::source_location::current());
    }
    if (instance._accessor == nullptr || !instance._accessor->instance) {
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Surface",
                "Surface creation requires a valid Vulkan instance.",
                std::source_location::current());
    }
	_data = std::make_shared<VSL_NAMESPACE::_impl::Surface_impl>();
	_data->vulkan = instance._accessor;
    _data->window = window->_data->window_handle;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(_data->vulkan->instance, (GLFWwindow*)_data->window, nullptr, &surface) != VK_SUCCESS) {
        const char * error_txt = nullptr;
        int error = glfwGetError(&error_txt);
        _data.reset();
        throw VSL_NAMESPACE::exceptions::RuntimeException(
                "Surface",
                std::string("Failed to create Window surface: ") +
                (error == GLFW_NO_ERROR || error_txt == nullptr ? "Unknown error" : error_txt),
                std::source_location::current());
    }
	_data->surface = vk::SurfaceKHR(surface);
	/*
	vk::Win32SurfaceCreateInfoKHR createInfo;
	createInfo.hwnd = glfwGetWin32Window((GLFWwindow*)data->window_handle);
	createInfo.hinstance = GetModuleHandle(nullptr);

	_data->surface = _data->instance.createWin32SurfaceKHR(createInfo);
	*/
}

VSL_NAMESPACE::_impl::Surface_impl::~Surface_impl()
{
    if (vulkan != nullptr && vulkan->instance && surface)
	    vulkan->instance.destroySurfaceKHR(surface);
    surface = nullptr;
}
