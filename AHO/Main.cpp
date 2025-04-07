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
#include <VSL/Vulkan/stages/shader_group.h>
#include <VSL/Vulkan/stages/color_blend.h>
#include <VSL/Vulkan/stages/input_assembly.h>
#include <VSL/Vulkan/stages/multisample.h>
#include <VSL/Vulkan/stages/rasterization.h>
#include <VSL/Vulkan/stages/vertex_input.h>
#include <VSL/Vulkan/pipeline.h>
#include <VSL/Vulkan/frame_buffer.h>
#include <VSL/Vulkan/commands/render_pass_begin.h>
#include <VSL/Vulkan/commands/render_pass_end.h>
#include <VSL/Vulkan/commands/bind_pipe_line.h>

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning( disable : 4455 )

#include <AHO/define.h>
#include <AHO/core/math/Mat.h>
#include <AHO/core/math/coordinate.h>
#include <AHO/core/math/angle.h>

#include <AHO/core/Vector.h>
#include <AHO/core/Point.h>
// #include <AHO/core/Triangle.h>

#include <chrono>

/*
* https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Command_buffers
*/

int main() {
	using namespace aho;
	using namespace aho::coordinate;
	using namespace aho::angle;

	namespace pl = vsl::pipeline_layout;

	auto x_z = 10_x + 10_z;

	x_z -= 5_z;

	auto x_y_z = x_z + 7_y;


	vsl::loggingln(x_z._cnmn1, ", ", x_z._cnmn2);
	vsl::loggingln(x_y_z.x, ", ", x_y_z.y, ", ", x_y_z.z);

	Vector vec1(1, 1);
	Vector vec2(2.0, 2.0);
	_Vector<decltype(10_x + 25_z)::element_type, decltype(10_x + 25_z)::coordinate_info::dimention, decltype(10_x + 25_z)::coordinate_info>
		x_z_vec(10_x + 25_z);
	auto vec3 = vec1 + vec2;

	vsl::loggingln(x_z_vec.value.x, ", ", x_z_vec.value.z);

	// Triangle triangle({ 0, 0 }, { 0, 4 }, { 2, 2 });


	// vsl::loggingln("area : ", triangle.area());

	try {
		using namespace vsl;

#ifdef _MSC_VER
		Vulkan vk("test", { "VK_KHR_win32_surface", "VK_KHR_surface" });
#elifdef __APPLE_CC__
        Vulkan vk("test", { "VK_KHR_portability_enumeration", "VK_KHR_surface", "VK_EXT_metal_surface" });
#endif
		Window main_window("vsl");
		auto physical_device = PhysicalDevices(vk).search();
		auto surface = main_window.addPlugin<Surface>(vk);

		LogicalDevice device(physical_device, surface);
		vsl::loggingln("selected : ", physical_device.name(), "(", physical_device.apiVersion(), ")");

		Swapchain swapchain(device);
		View view(swapchain);

#ifdef _MSC_VER
        vsl::Shader<ShaderType::Vertex> const_triangle_shader(device, "shaders/const_triangle.vert.spv");
		vsl::Shader<ShaderType::Fragment> red_shader(device, "shaders/red.frag.spv");
#elifdef __APPLE_CC__
        loggingln(std::filesystem::current_path());
        vsl::Shader<ShaderType::Vertex> const_triangle_shader(device, "../../AHO/shaders/const_triangle.vert.spv");
        vsl::Shader<ShaderType::Fragment> red_shader(device, "../../AHO/shaders/red.frag.spv");
#endif
		PipelineLayout layout(device,
			pl::ShaderGroup("red_triangle", { const_triangle_shader, red_shader }),
			pl::ColorBlend(),
			pl::InputAssembly(),
			pl::Multisample(),
			pl::Rasterization(),
			pl::VertexInput());

		RenderPass render_pass(swapchain);

		Pipeline pipeline(layout, render_pass);

		FrameBuffer frame_buffer(swapchain, view, render_pass);

		CommandManager manager(device);

		while (Window::Update() && main_window) {
			auto phase = manager.startPhase();
			phase << std::make_shared<command::RenderPassBegin>(render_pass, frame_buffer);
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