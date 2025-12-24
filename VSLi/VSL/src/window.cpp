#ifdef _MSC_VER
#include "pch.h"
#endif
#include <VSL/define.hpp>
#include <VSL/window.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <unistd.h>

#include <VSL/window_plugin.hpp>
#include <VSL/vulkan/surface.hpp>

std::vector<std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData>> VSL_NAMESPACE::PureWindow::_WINDOWS;
bool VSL_NAMESPACE::PureWindow::inited = false;

VSL_NAMESPACE::PureWindow::PureWindow(std::string _name, int _width, int _height)
{
	_data = std::shared_ptr<WindowData>(new WindowData);

	if (!inited) {
        if (glfwInit() == GLFW_TRUE) {
            inited = true;
        } else {
            const char *error_txt = nullptr;
            int error = glfwGetError(&error_txt);
            std::cerr << "Failed to initialize GLFW: " << (error == GLFW_NO_ERROR ? "Unknown error" : error_txt)
                      << std::endl;
            return; // TODO throw error
        }
	}
	_WINDOWS.push_back(_data);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
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
		for (auto& f : p->update)
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

bool vsl::PureWindow::resize(int width, int height) {
    glfwSetWindowSize((GLFWwindow*)_data->window_handle, width, height);
    return true;
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

vsl::Window::Window(std::shared_ptr<WindowData> data) : PureWindow(data) {
    if (not data->plugins.contains(vsl::window_plugin::QuietClose::QUIET_CLOSE_USED_ID))
        addPlugin<vsl::window_plugin::QuietClose>();
}