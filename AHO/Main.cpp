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
#include <VSL/Vulkan/pipeline_layout.h>
#include <VSL/Vulkan/pipeline.h>

// #define AHO_POOP_PUBLIC_SECURITY

#include <AHO/define.h>
#include <AHO/core/math/Mat.h>
#include <AHO/core/math/coordinate.h>
#include <AHO/core/math/angle.h>

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
	using namespace aho::angle;

	auto x_z = 10x + 10z;

	x_z -= 5z;
	auto x_y_z = x_z + 7y;
	

	vsl::loggingln(x_z._cnmn1, ", ",  x_z._cnmn2);
	vsl::loggingln(x_y_z.x, ", ", x_y_z.y, ", ", x_y_z.z);

	Vector vec1(1, 1);
	Vector vec2(2.0, 2.0);
	_Vector<int, vsl::D2, decltype(x + z)::coordinate_info> x_z_vec(10x + 25z);
	auto vec3 = vec1 + vec2;

	vsl::loggingln(x_z_vec.value.x, ", ", x_z_vec.value.z);

	Triangle triangle({ 0, 0 }, { 0, 4 }, { 2, 2 });


	vsl::loggingln("area : ", triangle.area());

	// Polygon();

	try {
		using namespace vsl;

		Vulkan vk("test", { "VK_KHR_win32_surface", "VK_KHR_surface" });
		Window main_window("main");
		auto physical_devices = PhysicalDevices(vk).search();
		auto surface = main_window.addPlugin<Surface<vsl::validation>>(vk);

		LogicalDevice device(physical_devices, surface);

		Swapchain swapchain(device);
		View view(swapchain);

		PipelineLayout layout(device);
		Pipeline pipeline(layout);

		pipeline;

		vsl::Shader<ShaderType::Vertex> const_triangle_shader(device, "shaders/const_triangle.vert.spv");
		vsl::Shader<ShaderType::Fragment> red_shader(device, "shaders/red.frag.spv");

		// pipeline << ShaderPipelineStage<ShaderPipelineStageType::Vertex>("triangle", const_triangle_shader);

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