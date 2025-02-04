#include "pch.h"
#include "define.h"
#include "Window.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "window_plugin.h"
#include "Vulkan/surface.h"

std::vector<std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData>> VSL_NAMESPACE::PureWindow::_WINDOWS;
bool VSL_NAMESPACE::PureWindow::inited = false;

VSL_NAMESPACE::PureWindow::PureWindow(std::string _name, int _width, int _height)
{
	_data = std::shared_ptr<WindowData>(new WindowData);

	if (!inited) {
		glfwInit();
		inited = true;
	}
	_WINDOWS.push_back(_data);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	_data->height = _height;
	_data->width = _width;
	_data->name = _name;
	_data->window_handle = (void*)glfwCreateWindow(_width, _height, _name.c_str(), nullptr, nullptr);
}

VSL_NAMESPACE::PureWindow::PureWindow(std::shared_ptr<WindowData> _data) : _data(_data) {}

bool VSL_NAMESPACE::PureWindow::Update()
{
	glfwPollEvents();
	bool s = true;
	for (auto p : _WINDOWS)
		for (auto& f : p->updateables)
			if (!f->onUpdate(PureWindow(p)))
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

bool VSL_NAMESPACE::PureWindow::operator==(const PureWindow& o)
{
	return _data == o._data;
}

bool VSL_NAMESPACE::PureWindow::operator==(const PureWindow::WindowData* o)
{
	return _data.get() == o;
}

void VSL_NAMESPACE::PureWindow::WindowData::destroy()
{
	for (auto i = _WINDOWS.begin(); i != _WINDOWS.end(); i++)
		if (i->get() == this) {
			_WINDOWS.erase(i);
			break;
		}
	glfwDestroyWindow((GLFWwindow*)window_handle);

	window_handle = nullptr;

	if (_WINDOWS.size() == 0) {
		glfwTerminate();
		inited = false;
	}
}

VSL_NAMESPACE::Window::Window(std::string name, int width, int height) : PureWindow(name, width, height) {
	addPlugin<vsl::window_plugin::QuietClose>();
}

VSL_NAMESPACE::Window::~Window() {
	close();
}