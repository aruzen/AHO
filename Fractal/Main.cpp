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
#include <VSL/Vulkan/phase.hpp>
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
#include <VSL/Vulkan/commands/bind_index_buffers.h>
#include <VSL/Vulkan/commands/bind_graphic_resource.hpp>
#include <VSL/Vulkan/commands/draw.h>
#include <VSL/Vulkan/commands/draw_indexed.h>
#include <VSL/Vulkan/commands/render_pass_begin.h>
#include <VSL/Vulkan/commands/render_pass_end.h>
#include <VSL/Vulkan/stages/color_blend.h>
#include <VSL/Vulkan/stages/depth_stencil.h>
#include <VSL/Vulkan/stages/dynamic_state.h>
#include <VSL/Vulkan/stages/input_assembly.h>
#include <VSL/Vulkan/stages/multisample.h>
#include <VSL/Vulkan/stages/rasterization.h>
#include <VSL/Vulkan/stages/resource_binding.h>
#include <VSL/Vulkan/stages/shader_group.h>
#include <VSL/Vulkan/stages/vertex_input.h>
#include <VSL/defaults.hpp>
#include <VSL/Vulkan/descriptor.hpp>

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning( disable : 4455 )

#include <AHO/define.h>
#include <AHO/core/math/Mat.h>
#include <AHO/core/math/coordinate.h>
#include <AHO/core/math/angle.h>

#include <AHO/core/Vector.h>
#include <AHO/core/Point.h>
#include <AHO/core/Triangle.h>
#include <AHO/core/Polygon.h>
#include <AHO/core/color.h>

#include <AHO/io/Key.hpp>
#include <AHO/io/KeyBoard.hpp>

#include <chrono>

/*
* https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer
*/

#include <boost/bimap.hpp>

int main() {
    using namespace aho;
    using namespace aho::coordinate;
    using namespace aho::literals;
    namespace pl = vsl::pipeline_layout;

    try {
        using namespace vsl;
#ifdef _MSC_VER
        vsl::utils::ShaderCompiler shader_compiler("glslc", "shaders/");
#elifdef __APPLE_CC__
        vsl::utils::ShaderCompiler shader_compiler("glslc", "../../AHO/shaders/");
#endif
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
                input_d3_shaders("d3_input",
                                           {make_shader<"../../AHO/shaders/3Dnormal.vert.spv">(device),
                                            make_shader<"../../AHO/shaders/colorfull.frag.spv">(device)}),
                input_d2_shaders("2d_input", {make_shader<"../../AHO/shaders/input.vert.spv">(device),
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
        manager.setDefault();

        auto vert_input = pl::VertexInput().add_shape({data_format::FloatVec2}).add_shape({data_format::FloatRGB});

        FrameBuffer frame_buffer(swapchain, view, render_pass);

        auto resourceBindingLayout = std::vector(device.getSwapImageSize(), ResourceBindingLayout(device, 0, ResourceType::UniformBuffer, ShaderType::Vertex));
        Pipeline input_vertices(layout.copy().add(pl::ResourceBinding(resourceBindingLayout), input_d3_shaders, vert_input), render_pass);

        auto imageAvailable = synchro_manager.createSemaphore("imageAvailable", manager.getBuffer().getSize()),
                renderFinished = synchro_manager.createSemaphore("renderFinished", manager.getBuffer().getSize());
        auto inFlight = synchro_manager.createFence("inFlight", manager.getBuffer().getSize(), true);

        /*
        vertex center = {{0.0_f_x + 0.0_f_y}, HSV(0.0f, 0.0f, 0.1f).rgb()};
        vertex top = {{0.0_f_x + -0.1_f_y}, HSV(0.0f, 1.0f, 0.5f).rgb()};
        vertex right = {{0.1_f_x + 0.1_f_y}, HSV(120.0f, 1.0f, 0.5f).rgb()};
        vertex left = {{-0.1_f_x + 0.1_f_y}, HSV(240.0f, 1.0f, 0.5f).rgb()};
         */

        struct ubo_t {
            alignas(16) Mat4x4<float> model;
            alignas(16) Mat4x4<float> view;
            alignas(16) Mat4x4<float> proj;
        } ubo;

        std::array<RGB, 4> colors = {
                RGB{1.0f, 0.0f, 0.0f},
                RGB{0.0f, 1.0f, 0.0f},
                RGB{0.0f, 0.0f, 1.0f},
                RGB{1.0f, 1.0f, 1.0f}
        };

        std::array<d2::PointF, 4> vertices = {
                d2::PointF{-0.5f, -0.5f},
                d2::PointF{0.5f, -0.5f},
                d2::PointF{0.5f, 0.5f},
                d2::PointF{-0.5f, 0.5f}
        };

        std::array<d2::PointF, 4> vertices2 = {
                d2::PointF{-0.5f, -0.5f},
                d2::PointF{0.5f, -0.5f},
                d2::PointF{0.5f, 0.5f},
                d2::PointF{-0.5f, 0.5f}
        };

        const std::array<uint32_t, 6> indices = {
                0, 1, 2, 2, 3, 0
        };

        using UboBuffer = Buffer<vsl::MemoryType::UniformBuffer, vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
        std::vector<UboBuffer> uboBuffers;
        uboBuffers.reserve(device.getSwapImageSize());
        std::generate_n(std::back_inserter(uboBuffers), device.getSwapImageSize(), [&]() { return UboBuffer(device, sizeof(ubo_t)); });
        GraphicResourceManager resourceManager(device);
        auto [pool, resource] = resourceManager.make(resourceBindingLayout, uboBuffers);

        DeviceLocalBuffer<vsl::MemoryType::VertexBuffer> colorBuffer(device, sizeof(RGB)*colors.size());
        DeviceLocalBuffer<vsl::MemoryType::VertexBuffer> vertBuffer(device, sizeof(d2::PointF)*vertices.size());
        DeviceLocalBuffer<vsl::MemoryType::VertexBuffer> vert2Buffer(device, sizeof(d2::PointF)*vertices2.size());
        DeviceLocalBuffer<vsl::MemoryType::IndexBuffer> indexBuffer(device, sizeof(uint32_t)*indices.size());

        StagingBuffer vertStagingBuffer(device, sizeof(d2::PointF)*vertices.size());
        {
            StagingBuffer colorStagingBuffer(device, sizeof(RGB)*colors.size());
            colorStagingBuffer.copy(colors);
            colorBuffer.copy(colorStagingBuffer);


            StagingBuffer indexStagingBuffer(device, sizeof(uint32_t)*indices.size());
            indexStagingBuffer.copy(indices);
            indexBuffer.copy(indexStagingBuffer);

            vertStagingBuffer.copy(vertices);
            vertBuffer.copy(vertStagingBuffer);
            vertStagingBuffer.copy(vertices2);
            vert2Buffer.copy(vertStagingBuffer);
        }

        InputManager input_manager(main_window);
        auto key1 = input_manager.get<input::Key>(input::KeyCode::Num1);
        auto key2 = input_manager.get<input::Key>(input::KeyCode::Num2);
        auto keyUp = input_manager.get<input::Key>(input::KeyCode::Up);
        auto keyDown = input_manager.get<input::Key>(input::KeyCode::Down);
        auto keyLeft = input_manager.get<input::Key>(input::KeyCode::Left);
        auto keyRight = input_manager.get<input::Key>(input::KeyCode::Right);

        auto* target = &vertices;
        auto* targetBuffer = &vertBuffer;

        while (Window::Update() && main_window && input_manager) {
            static auto startTime = std::chrono::high_resolution_clock::now();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
            ubo.model = matrix::make_rotation(time * 5.0_rad, Vector(0.0f, 0.0f, 1.0f));
            ubo.view = matrix::make_view(Point(2.0f, 2.0f, 2.0f), Point(0.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 1.0f));
            ubo.proj = matrix::make_identity<float, 4>();
            for (auto r : ubo.proj) {
                for (auto e: r)
                    std::cout << e << " ";
                std::cout << std::endl;
            }

            {
                auto phase = manager.startPhase(swapchain, imageAvailable, renderFinished, inFlight);
                frame_buffer.setTargetFrame(phase.getImageIndex());

                uboBuffers[phase.getImageIndex()].copy(ubo);
                resource[phase.getImageIndex()].update(uboBuffers[phase.getImageIndex()], 0);

                if (key1->up()) {
                    target = &vertices;
                    targetBuffer = &vertBuffer;
                }
                if (key2->up()) {
                    target = &vertices2;
                    targetBuffer = &vert2Buffer;
                }

                d2::VectorF move;
                if (keyUp->pressed()) {
                    move.value -= 0.05_f_y;
                }
                if (keyDown->pressed()) {
                    move.value += 0.05_f_y;
                }
                if (keyLeft->pressed()) {
                    move.value -= 0.05_f_x;
                }
                if (keyRight->pressed()) {
                    move.value += 0.05_f_x;
                }
                if (move.length() != 0) {
                    std::cout << vertices[0].value << ":" << vertices2[0].value << std::endl;
                    for (auto& v : *target)
                        v += move;
                    vertStagingBuffer.copy(*target);
                    targetBuffer->copy(vertStagingBuffer);
                }
                phase << command::RenderPassBegin(render_pass, frame_buffer);
                phase << command::BindGraphicResource(resource[phase.getImageIndex()], BindingDestination::Graphics, input_vertices);
                phase << input_vertices << command::BindVertexBuffer(vert2Buffer, colorBuffer) << command::BindIndexBuffer(indexBuffer) << command::DrawIndexed(indices.size());
                phase << input_vertices << command::BindVertexBuffer(vertBuffer, colorBuffer) << command::BindIndexBuffer(indexBuffer) << command::DrawIndexed(indices.size());
                phase << command::RenderPassEnd();
            }
            manager.next();
        }
        inFlight.wait();
        defaults::release();
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