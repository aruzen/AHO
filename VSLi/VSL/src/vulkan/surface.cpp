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

VSL_NAMESPACE::Surface::Surface(VSL_NAMESPACE::PureWindow* window, VSL_NAMESPACE::VulkanAccessor instance)
{
	_data = std::make_shared<VSL_NAMESPACE::_impl::Surface_impl>();
	_data->vulkan = instance._accessor;
    _data->window = window->_data->window_handle;

	VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(_data->vulkan->instance, (GLFWwindow*)_data->window, nullptr, &surface) != VK_SUCCESS) {
        const char * error_txt = nullptr;
        int error = glfwGetError(&error_txt);
        std::cerr << "Failed to create Window surface: " << (error == GLFW_NO_ERROR ? "Unknown error" : error_txt) << std::endl;
        return; // TODO throw error
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
	vulkan->instance.destroySurfaceKHR(surface);
}
