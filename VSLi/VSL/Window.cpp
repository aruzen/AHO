#include "pch.h"
#include "define.h"
#include "Window.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "window_plugin.h"
#include "Vulkan/surface.h"

std::vector<VSL_NAMESPACE::PureWindow::WindowData*> VSL_NAMESPACE::PureWindow::windows;
bool VSL_NAMESPACE::PureWindow::inited = false;

VSL_NAMESPACE::PureWindow::PureWindow(std::string _name, int _width, int _height)
{
	_data = std::shared_ptr<WindowData>(new WindowData);

	if (!inited) {
		glfwInit();
		inited = true;
	}
	windows.push_back(_data.get());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	_data->height = _height;
	_data->width = _width;
	_data->name = _name;
	_data->window_handle = (void*)glfwCreateWindow(_width, _height, _name.c_str(), nullptr, nullptr);
}

VSL_NAMESPACE::PureWindow::~PureWindow() {
	close();
}

bool VSL_NAMESPACE::PureWindow::Update()
{
	glfwPollEvents();
	bool s = true;
	for (auto p : windows)
		for (auto& f : p->updateables)
			if (!f->onUpdate(p))
				s = false;
	return s;
}

bool VSL_NAMESPACE::PureWindow::NoCheckUpdate()
{
	glfwPollEvents();
	return true;
}

bool VSL_NAMESPACE::PureWindow::close()
{
	if (_data == nullptr)
		return false;

	_data->destroy();

	return true;
}

std::string VSL_NAMESPACE::PureWindow::name()
{
	return _data->name;
}

VSL_NAMESPACE::PureWindow::operator bool()
{
	return _data->window_handle != nullptr;
}

void VSL_NAMESPACE::PureWindow::WindowData::destroy()
{
	for (auto i = windows.begin(); i != windows.end(); i++)
		if (*i == this) {
			windows.erase(i);
			break;
		}
	glfwDestroyWindow((GLFWwindow*)window_handle);

	window_handle = nullptr;

	if (windows.size() == 0) {
		glfwTerminate();
		inited = false;
	}
}

VSL_NAMESPACE::Window::Window(std::string name, int width, int height) : PureWindow(name, width, height) {
	addPlugin<vsl::window_plugin::ShouldClose>();
}