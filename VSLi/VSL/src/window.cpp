#include <VSL/define.hpp>
#include <VSL/window.hpp>
#include <VSL/exceptions.hpp>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifndef _MSC_VER
#include <unistd.h>
#else
#endif

#include <VSL/window_plugin.hpp>
#include <VSL/vulkan/surface.hpp>

std::vector<std::shared_ptr<VSL_NAMESPACE::PureWindow::WindowData>> VSL_NAMESPACE::PureWindow::WINDOWS;
bool VSL_NAMESPACE::PureWindow::inited = false;

namespace {
[[noreturn]] void throw_glfw_error(const char* operation)
{
    const char* description = nullptr;
    const int error = glfwGetError(&description);
    throw VSL_NAMESPACE::exceptions::RuntimeException("GLFW",
        std::string(operation) + ": " + (description ? description : "Unknown error") +
        " (error " + std::to_string(error) + ")");
}
} // namespace

VSL_NAMESPACE::PureWindow::PureWindow(std::string _name, int _width, int _height)
{
    _data = std::make_shared<WindowData>();
    _data->height = _height;
    _data->width = _width;
    _data->name = std::move(_name);

    if (!inited) {
        if (glfwInit() != GLFW_TRUE)
            throw_glfw_error("Failed to initialize GLFW");
        inited = true;
    }
    try {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        _data->window_handle = glfwCreateWindow(_width, _height, _data->name.c_str(), nullptr, nullptr);
        if (!_data->window_handle)
            throw_glfw_error("Failed to create GLFW window");
        WINDOWS.push_back(_data);
    } catch (...) {
        if (_data->window_handle) {
            glfwDestroyWindow(static_cast<GLFWwindow*>(_data->window_handle));
            _data->window_handle = nullptr;
        }
        if (WINDOWS.empty()) {
            glfwTerminate();
            inited = false;
        }
        throw;
    }
}

VSL_NAMESPACE::PureWindow::PureWindow(std::shared_ptr<WindowData> _data) : _data(_data) {}

bool VSL_NAMESPACE::PureWindow::Update()
{
	glfwPollEvents();
	bool s = true;
	for (auto p : WINDOWS)
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
	return _data && _data->window_handle != nullptr;
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
	for (auto i = WINDOWS.begin(); i != WINDOWS.end(); i++)
		if (i->get() == this) {
			WINDOWS.erase(i);
			break;
		}
	glfwDestroyWindow((GLFWwindow*)window_handle);

	window_handle = nullptr;

	if (WINDOWS.size() == 0) {
		glfwTerminate();
		inited = false;
	}
}

VSL_NAMESPACE::Window::Window(std::string name, int width, int height) : PureWindow(name, width, height) {
	add_plugin<vsl::window_plugin::QuietClose>();
}

vsl::Window::Window(std::shared_ptr<WindowData> data) : PureWindow(data) {
    if (not data->plugins.contains(vsl::window_plugin::QuietClose::QUIET_CLOSE_USED_ID))
        add_plugin<vsl::window_plugin::QuietClose>();
}
