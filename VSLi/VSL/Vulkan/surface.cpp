#include "pch.h"
#include "../define.h"
#include "_pimpls.h"

#include "surface.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

vsl::Surface::Surface(vsl::PureWindow* window, VSL_NAMESPACE::VulkanAccessor instance)
{
	_data = std::shared_ptr<vsl::_impl::Surface_impl>(new vsl::_impl::Surface_impl);
	_data->vulkan = instance._accessor;

	VkSurfaceKHR surface;
	glfwCreateWindowSurface(instance._accessor->instance, (GLFWwindow*)window->_data->window_handle, nullptr, &surface);
	_data->surface = vk::SurfaceKHR(surface);
	_data->window = window->_data->window_handle;
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