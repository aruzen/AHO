#ifdef _MSC_VER
#include "pch.h"
#endif
#include "window_plugin.h"
#include "debug.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

bool vsl::window_plugin::ShouldClose::onUpdate(vsl::PureWindow::WindowData* data)
{
	if (glfwWindowShouldClose((GLFWwindow*)data->window_handle)) {
		vsl::logging("destroy " + data->name + " by ShouldClosePlugin");
		data->destroy();
	}
	return true;
}
