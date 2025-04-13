#include <VSL/concepts.h>
#include <VSL/debug.h>
#include <VSL/define.h>
#include <VSL/dimention.h>
#include <VSL/exceptions.h>
#include <VSL/test.h>
#include <VSL/Type.h>
#include <VSL/Window.h>
#include <VSL/window_plugin.h>
#include <VSL/utils/ShaderCompiler.h>
#include <VSL/utils/VSLArray.h>
#include <VSL/Vulkan/buffer.h>
#include <VSL/Vulkan/command.h>
#include <VSL/Vulkan/device.h>
#include <VSL/Vulkan/frame_buffer.h>
#include <VSL/Vulkan/pipeline.h>
#include <VSL/Vulkan/pipeline_layout.h>
#include <VSL/Vulkan/pv.h>
#include <VSL/Vulkan/render_pass.h>
#include <VSL/Vulkan/scissor.h>
#include <VSL/Vulkan/shader.h>
#include <VSL/Vulkan/surface.h>
#include <VSL/Vulkan/swapchain.h>
#include <VSL/Vulkan/synchronize.h>
#include <VSL/Vulkan/view.h>
#include <VSL/Vulkan/viewport.h>
#include <VSL/Vulkan/Vulkan.h>
#include <VSL/Vulkan/commands/bind_pipe_line.h>
#include <VSL/Vulkan/commands/bind_vertex_buffers.h>
#include <VSL/Vulkan/commands/draw.h>
#include <VSL/Vulkan/commands/render_pass_begin.h>
#include <VSL/Vulkan/commands/render_pass_end.h>
#include <VSL/Vulkan/stages/color_blend.h>
#include <VSL/Vulkan/stages/depth_stencil.h>
#include <VSL/Vulkan/stages/dynamic_state.h>
#include <VSL/Vulkan/stages/input_assembly.h>
#include <VSL/Vulkan/stages/multisample.h>
#include <VSL/Vulkan/stages/rasterization.h>
#include <VSL/Vulkan/stages/shader_group.h>
#include <VSL/Vulkan/stages/vertex_input.h>

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning( disable : 4455 )

#include <AHO/define.h>
#include <AHO/core/math/Mat.h>
#include <AHO/core/math/coordinate.h>
#include <AHO/core/math/angle.h>

#include <AHO/core/Vector.h>
#include <AHO/core/Point.h>
#include <AHO/core/Triangle.h>
#include <AHO/core/color.h>

#include <chrono>

/*
* https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer
*/

int main() {
    using namespace aho;
    using namespace aho::literals;

    namespace pl = vsl::pipeline_layout;

    auto x_z = 10_x + 10_z;

    x_z -= 5_z;

    auto x_y_z = x_z + 7_y;


    vsl::loggingln(x_z._cnmn1, ", ", x_z._cnmn2);
    vsl::loggingln(x_y_z.x, ", ", x_y_z.y, ", ", x_y_z.z);

    Vector vec1(1, 1);
    Vector vec2(2.0, 2.0);
    _Vector<decltype(10_x + 25_z)::element_type, decltype(10_x + 25_z)::coordinate_info::dimention, decltype(10_x +
                                                                                                             25_z)::coordinate_info>
            x_z_vec(10_x + 25_z);
    auto vec3 = vec1 + vec2;

    vsl::loggingln(x_z_vec.value.x, ", ", x_z_vec.value.z);

    Triangle triangle(Point{1.0f, 1.0f}, {1.0f, 3.0f}, {3.0f, 1.0f});
    Point p1{0, 0}, p2{2, 2};
    Point pf1{0.0f, 0.0f};
    Line line(p1, p2);
    PtrLine pline(&p1, &p2);
    auto t = line + Point{1, 1};

    vsl::loggingln("area : ", triangle.area());
    vsl::loggingln(triangle.pos1.value.x);
    /*
    vsl::loggingln("type : ", typeid(t).name());
    vsl::loggingln("Point{ 0.0f, 0.0f }::graphic_type == FloatVec2 : ",
                   decltype(pf1)::graphic_type == vsl::data_format::FloatVec2);
    vsl::loggingln("DrawManip is available : ", vsl::command::is_manipulator<vsl::command::DrawManip>);
     */
    /*
    try {
        using namespace vsl;
        vsl::utils::ShaderCompiler shader_compiler("glslc", "shaders/");
        shader_compiler.load();
        shader_compiler.compile();

#ifdef _MSC_VER
        Vulkan vk("test", { "VK_KHR_win32_surface", "VK_KHR_surface" });
#elifdef __APPLE_CC__
        Vulkan vk("test", {"VK_KHR_portability_enumeration", "VK_KHR_surface",
                           "VK_EXT_metal_surface" });
#endif
        auto physical_device = PhysicalDevices(vk).search();

        Window main_window("vsl", 800, 700);
        auto surface = main_window.addPlugin<Surface>(vk);

        LogicalDevice device(physical_device, surface);
        for (auto device: PhysicalDevices(vk).get())
            loggingln(physical_device.name(), "(", physical_device.apiVersion(), ")");
        vsl::loggingln("selected : ", physical_device.name(), "(", physical_device.apiVersion(), ")");

        SynchroManager synchro_manager(device);

        Swapchain swapchain(device);

        View view(swapchain);
#ifdef _MSC_VER
        auto s1 = make_shader<"shaders/const_triangle.vert.spv">(device);
        auto s2 = make_shader<"shaders/red.frag.spv">(device);
        pl::ShaderGroup red_triangle_shaders("red_triangle", { s1, s2 }),
            colorfull_triangle_shaders("colorfull_triangle", { make_shader<"shaders/const_triangle2.vert.spv">(device), make_shader<"shaders/colorfull.frag.spv">(device) }),
            input_sahders("2d_input", { make_shader<"shaders/input.vert.spv">(device), make_shader<"shaders/input.frag.spv">(device) });
#elifdef __APPLE_CC__
        auto s1 = make_shader<"../../AHO/shaders/const_triangle.vert.spv">(device);
        auto s2 = make_shader<"../../AHO/shaders/red.frag.spv">(device);
        pl::ShaderGroup red_triangle_shaders("red_triangle", {s1, s2}),
                colorfull_triangle_shaders("colorfull_triangle",
                                           {make_shader<"../../AHO/shaders/const_triangle2.vert.spv">(device),
                                            make_shader<"../../AHO/shaders/colorfull.frag.spv">(device)}),
                input_sahders("2d_input", {make_shader<"../../AHO/shaders/input.vert.spv">(device),
                                           make_shader<"../../AHO/shaders/input.frag.spv">(device)});
#endif

        Scissor scissor(swapchain);
        Viewport viewport(swapchain);
        Viewport left_viewport(viewport), right_viewport(viewport);
        left_viewport.width /= 2;
        right_viewport.width /= 2;
        right_viewport.x = left_viewport.width;


        PipelineLayout layout(device,
                              pl::ColorBlend(),
                              pl::InputAssembly(),
                              pl::Multisample(),
                              pl::Rasterization(),
                              // pl::DynamicState(),
                              scissor,
                              viewport);

        RenderPass render_pass(swapchain);

        CommandManager manager(device);

        auto vert_input = pl::VertexInput().add_shape({data_format::FloatVec2, data_format::FloatRGB});

        FrameBuffer frame_buffer(swapchain, view, render_pass);;
        Pipeline input_vertices(layout.copy().add(input_sahders, vert_input), render_pass);

        auto imageAvailable = synchro_manager.createSemaphore("imageAvailable", manager.getBuffer().getSize()),
                renderFinished = synchro_manager.createSemaphore("renderFinished", manager.getBuffer().getSize());
        auto inFlight = synchro_manager.createFence("inFlight", manager.getBuffer().getSize(), true);

        struct vertex {
            d2::PointF pos;
            RGB color;
        };

        vertex center = {{0.0_f_x + 0.0_f_y}, HSV(0.0f, 0.0f, 0.1f).rgb()};
        vertex top = {{0.0_f_x + -0.1_f_y}, HSV(0.0f, 1.0f, 0.5f).rgb()};
        vertex right = {{0.1_f_x + 0.1_f_y}, HSV(120.0f, 1.0f, 0.5f).rgb()};
        vertex left = {{-0.1_f_x + 0.1_f_y}, HSV(240.0f, 1.0f, 0.5f).rgb()};

        std::cout << center.color << std::endl;

        using SimpleBuffer = Buffer<MemoryType::VertexBuffer,
                MemoryProperty::HostVisible | MemoryProperty::HostCoherent>;
        std::array<SimpleBuffer, 3>
                buffers = {SimpleBuffer(device, sizeof(std::array<vertex, 3>)),
                           SimpleBuffer(device, sizeof(std::array<vertex, 3>)),
                           SimpleBuffer(device, sizeof(std::array<vertex, 3>))};

        size_t frame = 0;
        while (Window::Update() && main_window) {
            {
                auto phase = manager.startPhase(swapchain, imageAvailable, renderFinished, inFlight);
                frame_buffer.setTargetFrame(phase.getImageIndex());

                std::array<std::array<vertex, 3>, 3> polygons = {
                        std::array<vertex, 3>{top, right, center},
                        std::array<vertex, 3>{center, right, left},
                        std::array<vertex, 3>{top, center, left}
                };
                phase << command::RenderPassBegin(render_pass, frame_buffer);

                // phase << scissor << viewport;
                phase << input_vertices;
                for (size_t i = 0; i < 3; i++) {
                    buffers[i].copy(polygons[i]);
                    phase << command::BindVertexBuffer(buffers[i]) << command::Draw(3);
                }
                phase << command::RenderPassEnd();
            }
            frame++;
            manager.next();
        }
        inFlight.wait();
    }
    catch (std::exception &e) {
        vsl::loggingln(e.what());
        return 1;
    }
    catch (vsl::exceptions::VSLException &e) {
        vsl::loggingln(e.what());
        return 1;
    }
    /**/
}