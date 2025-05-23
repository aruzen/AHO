#ifdef _MSC_VER
#include "pch.h"
#endif
#include "window_plugin.h"
#include "debug.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

bool vsl::window_plugin::QuietClose::onUpdate(vsl::PureWindow window)
{
	auto data = window._data;
	if (glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
		vsl::logging("destroy " + data->name + " by QuietClosePlugin");
		data->destroy();
	}
	return true;
}

bool vsl::window_plugin::HookShouldClose::onUpdate(vsl::PureWindow window) {
	auto&& data = window._data;
	if (glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
		callback(window);
	}
	return true;
}

void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
	for (auto& wd : VSL_NAMESPACE::PureWindow::_WINDOWS) {
		if ((GLFWwindow*)wd->window_handle != window)
			continue;
		if (!wd->plugins.contains(vsl::window_plugin::HookResize::HOOK_RESIZE_USED_ID))
			return;
		auto p = wd->plugins[vsl::window_plugin::HookResize::HOOK_RESIZE_USED_ID];
		if (auto hook = std::dynamic_pointer_cast<vsl::window_plugin::HookResize>(p); hook)
			hook->callback(vsl::PureWindow(wd), width, height);
	}
}

vsl::window_plugin::HookResize::HookResize(vsl::PureWindow* window, std::function<void(PureWindow, int, int)> callback) :  callback(callback) {
	id = HOOK_RESIZE_USED_ID;
	glfwSetFramebufferSizeCallback((GLFWwindow*)window->_data->window_handle, framebuffer_resize_callback);
}
