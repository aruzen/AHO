#include <Test/tests.h>

#include <VSL/define.h>
#include <VSL/debug.h>
#include <VSL/Type.h>
#include <VSL/exceptions.h>

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
#include <VSL/Vulkan/shader.h>
#include <VSL/Vulkan/pipeline.h>

// #define AHO_POOP_PUBLIC_SECURITY

#include <AHO/define.h>
#include <AHO/core/math/Mat.h>
#include <AHO/core/math/coordinate.h>

#include <AHO/core/Vector.h>
#include <AHO/core/Point.h>
#include <AHO/core/Triangle.h>

#include <chrono>

/*
* https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
*/

int main() {
	using namespace aho;
	using namespace aho::coordinate;

	auto x_z = 10x + 10z;

	x_z -= 5z;
	auto x_y_z = x_z + 7y;
	
	vsl::loggingln(x_z._cnmn1, ", ",  x_z._cnmn2);
	vsl::loggingln(x_y_z.x, ", ", x_y_z.y, ", ", x_y_z.z);

	try {
		using namespace vsl;
		Vulkan vk("test", { "VK_KHR_win32_surface", "VK_KHR_surface" });
		Window main_window("main");
		auto pd = PhysicalDevices(vk).search();
		auto surface = main_window.addPlugin<Surface<vsl::validation>>(vk);

		LogicalDevice ld(pd, surface);

		Swapchain swapchain(ld);
		View view(swapchain);

		Pipeline pipeline;

		vsl::Shader const_triangle_shader(ld, "shaders/const_triangle.vert.spv");
		vsl::Shader red_shader(ld, "shaders/red.frag.spv");

		while (Window::Update() && main_window) {

		}
	}
	catch (std::exception& e) {
		vsl::loggingln(e.what());
		return 1;
	}
	catch (vsl::exceptions::VSLException& e) {
		vsl::loggingln(e.what());
		return 1;
	}
	/**/
}