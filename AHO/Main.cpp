#include <Test/tests.h>

#include <VSL/define.h>
#include <VSL/debug.h>
#include <VSL/Type.h>

#include <VSL/utils/ShaderCompiler.h>
#include <VSL/utils/VSLArray.h>

#include <VSL/Window.h>
#include <VSL/window_plugin.h>
#include <VSL/Vulkan/Vulkan.h>
#include <VSL/Vulkan/device.h>
#include <VSL/Vulkan/surface.h>
#include <VSL/Vulkan/command.h>
#include <VSL/Vulkan/swapchain.h>
#include <VSL/Vulkan/view.h>

#include <AHO/core/Vector.h>
#include <AHO/core/Point.h>
#include <AHO/core/Triangle.h>

#include <chrono>

/*
* https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
*/

struct MyPoint {
	using element_type = double;
	double x, y;
};

int main() {
	using namespace vsl;
	using namespace aho;

	constexpr size_t N = 10000;

	// AHO object
	vsl::loggingln(vsl::__VSLD2Point<aho::d2::Point<int>>::SPECIALIZED_ID);
	{
		auto begin = std::chrono::system_clock::now();
		aho::d2::Point p(1.1, 2.1);
		for (int i = 0; i < N; i++) {
			aho::d2::Point point(1.0, 2.1);
			vsl::__VSLD2Point<decltype(point)> vsl_point(&point);
			vsl_point.y = 0.25 + i;
			p.x += vsl_point.x;
		}
		vsl::loggingln(std::chrono::system_clock::now() - begin, p.x);
	}
	// single object
	vsl::loggingln(vsl::__VSLD2Point<MyPoint>::SPECIALIZED_ID);
	{
		auto begin = std::chrono::system_clock::now();
		aho::d2::Point p(1.1, 2.1);
		for (int i = 0; i < N; i++) {
			MyPoint point(1.0, 2.1);
			vsl::__VSLD2Point<decltype(point)> vsl_point(&point);
			vsl_point.y = 0.25 + i;
			p.x += vsl_point.x;
		}
		vsl::loggingln(std::chrono::system_clock::now() - begin, p.x);
	}
	// no vsl
	{
		auto begin = std::chrono::system_clock::now();
		aho::d2::Point p(1.1, 2.1);
		for (int i = 0; i < N; i++) {
			MyPoint point(1.0, 2.1);
			point.y = 0.25 + i;
			p.x += point.x;
		}
		vsl::loggingln(std::chrono::system_clock::now() - begin, p.x);
	}
	system("pause");
	system("cls");
	/*
	Vulkan vk("test", { "VK_KHR_win32_surface", "VK_KHR_surface" });
	Window main_window("main");
	auto pd = PhysicalDevices(vk).search();
	auto surface = main_window.addPlugin<Surface<vsl::validation>>(vk);
	LogicalDevice ld(pd, surface);
	Swapchain swapchain(ld);
	View view(swapchain);
	while (Window::Update() && main_window) {

	} */
}