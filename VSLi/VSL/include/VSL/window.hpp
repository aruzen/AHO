#pragma once
#include "define.hpp"

#include "concepts.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <string>

namespace VSL_NAMESPACE {
	namespace window_plugin {
		struct QuietClose;
	}

	class PureWindow
	{
	public:
		struct Plugin {
			unsigned char id = 0;
			virtual ~Plugin() {};
		};

		struct WindowData;
		struct OnUpdate {
			virtual bool onUpdate(vsl::PureWindow) = 0;
		};

		struct WindowData {
			unsigned char plugins_next_id = 0;
			std::map<unsigned char, std::shared_ptr<Plugin>> plugins;
			std::vector<std::shared_ptr<OnUpdate>> update;

			// void* plugdata = nullptr;
			void* window_handle = nullptr;
			std::string name;
			int width, height;

			void destroy();
		};

		static std::vector<std::shared_ptr<WindowData>> WINDOWS;
	private:
		static bool inited;
	public:
		PureWindow(std::string name, int width = 800, int height = 600);
		PureWindow(std::shared_ptr<WindowData> _data);

		static bool Update();
		static bool NoCheckUpdate();

		template<typename T, typename... Args>
		//	requires vsl::concepts::initializer<T, VSL_NAMESPACE::PureWindow::WindowData*, Args&&...>&& std::derived_from<T, VSL_NAMESPACE::PureWindow::Plugin>
		std::shared_ptr<T> addPlugin(Args&&... args);

        virtual bool resize(int width, int height);

		std::string name();
		bool close();

		operator bool();
		bool operator ==(const PureWindow& o);
		bool operator ==(const PureWindow::WindowData* o);

		std::shared_ptr<WindowData> _data;
	};

	class Window : public PureWindow {
	public:
		Window(std::string name, int width = 800, int height = 600);
        Window(std::shared_ptr<WindowData> data);
	};


	template<typename T, typename ...Args>
	// requires vsl::concepts::initializer<T, VSL_NAMESPACE::PureWindow::WindowData*, Args&&...>&& std::derived_from<T, VSL_NAMESPACE::PureWindow::Plugin>
	inline std::shared_ptr<T> VSL_NAMESPACE::PureWindow::addPlugin(Args && ...args)
	{
		auto t = std::shared_ptr<T>(new T(this, std::forward<Args>(args)...));
		std::shared_ptr<Plugin> p = std::static_pointer_cast<Plugin>(t);

		if (auto o = std::dynamic_pointer_cast<OnUpdate>(p); o)
			_data->update.push_back(o);

		if (t->id == 0)
			p->id = _data->plugins_next_id;
		_data->plugins[t->id] = p;
		_data->plugins_next_id++;

		return t;
	}
}