#include <VSL/vsl.hpp>
#include <VSL/utils/shader_compiler.hpp>
#include <VSL/utils/spir_reflector.hpp>

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning( disable : 4455 )

#include <AHO/define.hpp>
#include <AHO/core/math/mat.hpp>
#include <AHO/core/math/coordinate.hpp>
#include <AHO/core/math/angle.hpp>

#include <AHO/core/vector.hpp>
#include <AHO/core/point.hpp>
#include <AHO/core/triangle.hpp>
#include <AHO/core/polygon.hpp>
#include <AHO/core/color.hpp>

#include <AHO/io/key.hpp>
#include <AHO/io/keyBoard.hpp>
#include <AHO/io/mouse.hpp>
#include <AHO/engine.hpp>
#include <AHO/engine/standard_engine.hpp>

#include <chrono>
#include <print>
#include <cstdlib>

/*
* https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer
*/

#include <boost/bimap.hpp>
#include "../thirdparty/stb_image.h"

int main() {
    using namespace aho;
    using namespace aho::coordinate;
    using namespace aho::literals;
    namespace pl = vsl::pipeline_layout;
    namespace gfx_src = vsl::graphic_resource;

#if !defined(DEBUG) && !defined(_DEBUG)
    vsl::loggingln("Release build");
    try
#else
    vsl::loggingln("Debug build");
#endif
    {
        /*
breakpoint set --name swift_willThrow
breakpoint disable swift_willThrow
         /opt/homebrew/bin/cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja -S /Users/morimoto_hibiki/repos/AHO -B .
         */
        using namespace vsl;
        aho::engine::StandardEngine engine("test");

        auto &[vulkan_instance, physical_device, device, command_manager, graphic_resource_manager, synchro_manager]
                = *engine._data;
        auto &main_window = engine.boot_window.value();
        auto &[surface, swapchain, image_view, render_pass, frame_buffer]
                = *engine.boot_window.value()._data2;
#ifdef _MSC_VER
        vsl::utils::ShaderCompiler shader_compiler("glslc", "shaders/");
#elifdef __APPLE_CC__
        vsl::utils::ShaderCompiler shader_compiler("glslc",
                                                   {"../../AHO/shaders/", "${AHO_HOME}/built-in-resource/shaders/"});
#endif
        shader_compiler.load();
        shader_compiler.compile();

        // Window main_window("vsl", 800, 800);
        // auto surface = main_window.addPlugin<Surface>(vulkan_instance);

#ifdef _MSC_VER
        auto s1 = make_shader<"shaders/const_triangle.vert.spv">(device);
        auto s2 = make_shader<"shaders/red.frag.spv">(device);
        pl::ShaderGroup red_triangle_shaders("red_triangle", { s1, s2 }),
            colorfull_triangle_shaders("colorfull_triangle", { make_shader<"shaders/const_triangle2.vert.spv">(device), make_shader<"shaders/colorfull.frag.spv">(device) }),
            input_sahders("2d_input", { make_shader<"shaders/input.vert.spv">(device), make_shader<"shaders/input.frag.spv">(device) });
#elifdef __APPLE_CC__
        pl::ShaderGroup input_d3_shaders("3d_color_input",
                                         {make_shader<"${AHO_HOME}/built-in-resource/shaders/vd2p_fc_umpv.vert.spv">(
                                                 device),
                                          make_shader<"${AHO_HOME}/built-in-resource/shaders/fc.frag.spv">(device)}),
                input_d2_shaders("2d_color_input",
                                 {make_shader<"${AHO_HOME}/built-in-resource/shaders/vd2p_fc.vert.spv">(device),
                                  make_shader<"${AHO_HOME}/built-in-resource/shaders/fc.frag.spv">(device)}),
                push_d2_shaders("2d_color_push",
                                {make_shader<"${AHO_HOME}/built-in-resource/shaders/2dtriangle_single_color.vert.spv">(
                                        device),
                                 make_shader<"${AHO_HOME}/built-in-resource/shaders/fc.frag.spv">(device)}),
                input_texture_shaders("input_texture",
                                      {make_shader<"../../AHO/shaders/texture.vert.spv">(device),
                                       make_shader<"../../AHO/shaders/texture.frag.spv">(device)});
#endif

        // utils::SPIRVReflector reflector(device, std::filesystem::path(expand_environments("${AHO_HOME}/built-in-resource/shaders/all-types.vert.spv")));

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
                              pl::DepthStencil(),
                // pl::DynamicState(),
                              scissor,
                              viewport);

        auto [input_vertices_pool, input_vertices_resource, input_vertices_resource_layout, input_vertices] = [&]() {
            auto vd2p_fc_umpv_reflect
                    = utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments(
                                    "${AHO_HOME}/built-in-resource/shaders/vd2p_fc_umpv.vert.spv"))).generated;
            auto input_vertices_resource_layout = vd2p_fc_umpv_reflect.makeBindingLayout();
            return std::tuple_cat(graphic_resource_manager->allocate(
                                          std::vector(swapchain.getSwapImageSize(), input_vertices_resource_layout[0])),
                                  std::make_tuple(input_vertices_resource_layout, GraphicsPipeline(
                                          layout.copy().add(
                                                  pl::ResourceBinding(input_vertices_resource_layout),
                                                  input_d3_shaders, vd2p_fc_umpv_reflect.vertex_input),
                                          render_pass)));
        }();

        auto [push_triangle_layout, push_triangle] = [&]() {
            auto d2triangle_single_color
                    = utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments(
                                    "${AHO_HOME}/built-in-resource/shaders/2dtriangle_single_color.vert.spv")
                    )).generated;
            auto push_triangle_layout = layout.copy().add(
                    d2triangle_single_color.push_constants,
                    push_d2_shaders);
            return std::make_tuple(push_triangle_layout, GraphicsPipeline(
                    push_triangle_layout,
                    render_pass));
        }();

        auto [texture_pool, texture_resource, texture_resource_layout, input_texture] = [&]() {
            auto texture_vert
                    = utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments("../../AHO/shaders/texture.vert.spv"))).generated;
            auto vert_resource_layout = texture_vert.makeBindingLayout()[0];
            auto texture_frag
                    = utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments("../../AHO/shaders/texture.frag.spv"))).generated;
            auto frag_resource_layout = texture_frag.makeBindingLayout()[0];
            std::vector l(swapchain.getSwapImageSize(), vert_resource_layout);
            l.push_back(frag_resource_layout);
            return std::tuple_cat(graphic_resource_manager->allocate(l),
                                  std::make_tuple(std::vector{vert_resource_layout, frag_resource_layout},
                                                  GraphicsPipeline(
                                                          layout.copy().add(
                                                                  pl::ResourceBinding(
                                                                          {vert_resource_layout, frag_resource_layout}),
                                                                  input_texture_shaders, texture_vert.vertex_input),
                                                          render_pass)));
        }();


        auto imageAvailable = synchro_manager.createSemaphore("imageAvailable",
                                                              command_manager.getBuffer().getSize()),
                renderFinished = synchro_manager.createSemaphore("renderFinished",
                                                                 command_manager.getBuffer().getSize());
        auto inFlight = synchro_manager.createFence("inFlight",
                                                    command_manager.getBuffer().getSize(), true);

        struct alignas(16) ubo_t {
            Mat4x4<float> model;
            Mat4x4<float> view;
            Mat4x4<float> proj;
        } ubo;

        std::array<RGB, 4> colors = {
                RGB{1.0f, 0.0f, 0.0f},
                RGB{0.0f, 1.0f, 0.0f},
                RGB{0.0f, 0.0f, 1.0f},
                RGB{1.0f, 1.0f, 1.0f}
        };

        std::array<d2::PointF, 4> vertices = {
                d2::PointF{0.5f, -0.5f},
                d2::PointF{-0.5f, -0.5f},
                d2::PointF{-0.5f, 0.5f},
                d2::PointF{0.5f, 0.5f}
        };

        std::array<d2::PointF, 4> texcoords = {
                d2::PointF{1.0f, 0.0f},
                d2::PointF{0.0f, 0.0f},
                d2::PointF{0.0f, 1.0f},
                d2::PointF{1.0f, 1.0f}
        };

        const std::array<uint32_t, 6> indices = {
                0, 1, 2, 2, 3, 0
        };

        struct alignas(16) debug_t {
            std::array<float, 4 * 3> v;
        } *debug;

        using UboBuffer = Buffer<vsl::MemoryType::UniformBuffer,
                vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
        std::vector<UboBuffer> uboBuffers;
        uboBuffers.reserve(swapchain.getSwapImageSize());
        std::generate_n(std::back_inserter(uboBuffers), swapchain.getSwapImageSize(),
                        [&]() { return UboBuffer(device, sizeof(ubo_t)); });

        for (size_t i = 0; i < input_vertices_resource.size() && i < uboBuffers.size(); i++) {
            input_vertices_resource[i].update(uboBuffers[i], (size_t) 0);
            texture_resource[i].update(uboBuffers[i], (size_t) 0);
        }
        DeviceLocalBuffer<vsl::MemoryType::VertexBuffer> colorBuffer(device, command_manager, colors);
        DeviceLocalBuffer<vsl::MemoryType::IndexBuffer> indexBuffer(device, command_manager, indices);
        DeviceLocalBuffer<vsl::MemoryType::VertexBuffer> vertBuffer(device, command_manager, vertices);
        DeviceLocalBuffer<vsl::MemoryType::VertexBuffer> texCoordBuffer(device, command_manager, texcoords);

        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(expand_environments("${AHO_HOME}/built-in-resource/images/ahahacraft.png").c_str(),
                                    &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        Image<ImageType::TransferDst | ImageType::Sampled> image(device, texWidth, texHeight);
        Sampler sampler(device);
        {
            StagingBuffer<> imageStagingBuffer(device, texHeight * texWidth * texChannels);
            auto data = imageStagingBuffer.data();
            memcpy(data.data, pixels, texHeight * texWidth * texChannels);
            {
                auto phase = command_manager.startPhase<ComputePhase>(std::nullopt, std::nullopt, inFlight);
                phase << command::ChangeImageBarrier(image, ImageLayout::TransferDstOptimal);
                phase << command::CopyBufferToImage(image, &imageStagingBuffer, ImageLayout::TransferDstOptimal);
                phase << command::ChangeImageBarrier(image, ImageLayout::TransferDstOptimal,
                                                     ImageLayout::ShaderReadOnlyOptimal);
            }
            inFlight.wait();
        }
        texture_resource[swapchain.getSwapImageSize()].update(image, sampler, 1);


        Buffer<vsl::MemoryType::StorageBuffer,
                vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>
                debugBuffer(device, sizeof(debug_t));

        InputManager input_manager(main_window);
        auto [keyUp,
                keyDown,
                keyLeft,
                keyRight] = input_manager.get<input::Keys>(
                input::KeyCode::Up,
                input::KeyCode::Down,
                input::KeyCode::Left,
                input::KeyCode::Right)->keys;
        auto mouse = input_manager.get<input::Mouse>();

        d3::VectorF move;
        while (aho::Window::Update() && main_window && input_manager) {
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();

            if (keyUp->pressed())
                move.value -= 0.05_f_y;
            if (keyDown->pressed())
                move.value += 0.05_f_y;
            if (keyLeft->pressed())
                move.value -= 0.05_f_x;
            if (keyRight->pressed())
                move.value += 0.05_f_x;

            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
            ubo.model = matrix::make_rotation(time * 5.0_rad, Vector(0.0f, 0.0f, 1.0f)) * matrix::make_move(move);
            ubo.view = matrix::make_view(Point(2.0f, 2.0f, 2.0f), Point(0.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 1.0f));
            ubo.proj = matrix::make_perspective(45.0_deg, viewport.width / (float) viewport.height, 0.1f, 10.0f);

            {
                auto phase = command_manager.startPhase(swapchain, imageAvailable, renderFinished, inFlight);
                frame_buffer.setTargetFrame(phase.getImageIndex());
                uboBuffers[phase.getImageIndex()].copy(ubo);
                input_vertices_resource[phase.getImageIndex()].update(uboBuffers[phase.getImageIndex()], 0);
                texture_resource[phase.getImageIndex()].update(uboBuffers[phase.getImageIndex()], 0);

                Triangle<float, vsl::D2> triangle{Point(-0.5f, 0.0f), Point(0.5f, 0.5f), Point(-0.5f, -0.5f)};
                RGB rgb{1.0f, 0.0f, 0.0f};
                phase << command::RenderPassBegin(render_pass, frame_buffer);
                /*
                phase << input_vertices
                      << command::BindGraphicResource(input_vertices_resource[phase.getImageIndex()],
                                                      graphic_resource::BindingDestination::Graphics, input_vertices)
                      << command::BindVertexBuffer(vertBuffer, colorBuffer)
                      << command::BindIndexBuffer(indexBuffer)
                      << command::DrawIndexed(indices.size());
                      */

                phase << push_triangle
                      << command::PushConstant(push_triangle_layout, ShaderFlag::Vertex, sizeof triangle, 0, &triangle)
                      << command::PushConstant(push_triangle_layout, ShaderFlag::Vertex, sizeof rgb, 44 - sizeof rgb, &rgb)
                      << command::Draw(3);

                phase << input_texture
                      << command::BindGraphicResource(
                              {input_vertices_resource[phase.getImageIndex()], texture_resource[swapchain.getSwapImageSize()]},
                              graphic_resource::BindingDestination::Graphics, input_texture)
                      << command::BindVertexBuffer(vertBuffer, colorBuffer, texCoordBuffer)
                      << command::BindIndexBuffer(indexBuffer)
                      << command::DrawIndexed(indices.size());

                phase << command::RenderPassEnd();
            }
            // auto debugMapped = debugBuffer.data();
            // debug = static_cast<struct debug_t *>(debugMapped.data);
            command_manager.next();
        }
        inFlight.wait();
        /**/
    }
#if !defined(DEBUG) && !defined(_DEBUG)
    catch (std::exception &e) {
        vsl::loggingln(e.what());
        return 1;
    }
    catch (vsl::exceptions::VSLException &e) {
        vsl::loggingln(e.what());
        return 1;
    }
#endif
    /**/
}
/* AHO/shaders/raw/vd2p_fc_umpv.vert AHOLi/AHO/object/Object.hpp AHOLi/AHO/resource/Audio.hpp AHOLi/AHO/resource/image.hpp AHOLi/AHO/resource/ObjectHitbox.hpp AHOLi/AHO/resource/ObjectModel.hpp VSLi/VSL/Vulkan/commands/bind_graphic_resource.cpp VSLi/VSL/Vulkan/commands/bind_graphic_resource.hpp VSLi/VSL/Vulkan/descriptor.cpp VSLi/VSL/Vulkan/descriptor.hpp VSLi/VSL/Vulkan/phase.cpp VSLi/VSL/Vulkan/phase.hpp VSLi/VSL/Vulkan/stages/resource_binding.cpp VSLi/VSL/Vulkan/stages/resource_binding.h
 */