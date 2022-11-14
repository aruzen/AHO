#pragma once
#include "define.h"

#include "concepts.h"

#include <vector>
#include <functional>
#include <map>
#include <string>

namespace VSL_NAMESPACE {
	namespace window_plugin {
		struct ShouldClose;
	}

	class PureWindow
	{
	public:
		struct Plugin {
			unsigned char id = 0;
			virtual ~Plugin() {};
		};

		struct WindowData;
		struct OnUpdateable {
			virtual bool onUpdate(vsl::PureWindow::WindowData * data) = 0;
		};

		struct WindowData {
			unsigned char plugins_next_id = 0;
			std::map<char, std::shared_ptr<Plugin>> plugins;
			std::vector<std::shared_ptr<OnUpdateable>> updateables;

			void* plugdata = nullptr;
			void* window_handle = nullptr;
			std::string name;
			int width, height;

			void destroy();
		};
	private:
		static std::vector<WindowData*> windows;
		static bool inited;
	public:
		PureWindow(std::string name, int width = 800, int height = 600);
		~PureWindow();

		static bool Update();
		static bool NoCheckUpdate();

		template<typename T, typename... Args>
			requires vsl::concepts::initializer<T, VSL_NAMESPACE::PureWindow::WindowData*, Args&&...>&& std::derived_from<T, VSL_NAMESPACE::PureWindow::Plugin>
		std::shared_ptr<T> addPlugin(Args&&... args);

		bool close();
		std::string name();

		operator bool();

	private:
		std::shared_ptr<WindowData> _data;
	};

	class Window : public PureWindow{
	public:
		Window(std::string name, int width = 800, int height = 600);
	};


	template<typename T, typename ...Args>
	requires vsl::concepts::initializer<T, VSL_NAMESPACE::PureWindow::WindowData*, Args&&...>&& std::derived_from<T, VSL_NAMESPACE::PureWindow::Plugin>
	inline std::shared_ptr<T> VSL_NAMESPACE::PureWindow::addPlugin(Args && ...args)
	{
		auto t = std::shared_ptr<T>(new T(_data.get(), std::forward<Args>(args)...));
		auto p = std::static_pointer_cast<Plugin>(t);
		if constexpr (std::derived_from<T, vsl::PureWindow::OnUpdateable>) {
			auto o = std::static_pointer_cast<OnUpdateable>(t);
			_data->updateables.push_back(o);
		}

		t->id = _data->plugins_next_id;
		_data->plugins[_data->plugins_next_id] = p;
		_data->plugins_next_id++;

		return t;
	}
}