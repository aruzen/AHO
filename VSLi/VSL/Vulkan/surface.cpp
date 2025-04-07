#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"
#include "_pimpls.h"

#include "surface.h"

#ifdef _MSC_VER
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE_CC__)
#define GLFW_EXPOSE_NATIVE_MACOS
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

vsl::Surface::Surface(vsl::PureWindow::WindowData* data, VSL_NAMESPACE::VulkanAccessor instance)
{
	_data = std::shared_ptr<vsl::_impl::Surface_impl>(new vsl::_impl::Surface_impl);
	_data->vulkan = instance._accessor;
	VkSurfaceKHR surface;

    if (glfwCreateWindowSurface(instance._accessor->instance, (GLFWwindow*)data->window_handle, nullptr, &surface) != VK_SUCCESS) {
        const char * error_txt = nullptr;
        int error = glfwGetError(&error_txt);
        std::cerr << "Failed to create window surface: " << (error == GLFW_NO_ERROR ? "Unknown error" : error_txt) << std::endl;
        return; // TODO throw error
    }
    _data->surface = vk::SurfaceKHR(surface);
	_data->window = data->window_handle;
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
