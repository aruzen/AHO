#include <VSL/vsl.hpp>
#include <VSL/utils/shader_compiler.hpp>
#include <VSL/utils/spir_reflector.hpp>

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning( disable : 4455 )

#include <AHO/aho.hpp>

#include <chrono>
#include <print>
#include <cstdlib>

/*
* https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer
*/

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
        auto &[surface, swapchain, render_pass,
                frame_buffer, image_available, render_finish, in_flight]
                = *engine.boot_window.value()._data2;
        if (!image_available._data)
            loggingln("image not available ; ;");
        main_window.resize(500, 500);
#ifdef _MSC_VER
        vsl::utils::ShaderCompiler shader_compiler("glslc", { "shaders/", "${AHO_HOME}/built-in-resource/shaders/" });
#elif defined(__APPLE_CC__)
        vsl::utils::ShaderCompiler shader_compiler("glslc",
                                                   {"../../AHO/shaders/", "${AHO_HOME}/built-in-resource/shaders/"});
#endif
        shader_compiler.load();
        shader_compiler.compile();

        // Window main_window("vsl", 800, 800);
        // auto surface = main_window.add_plugin<Surface>(vulkan_instance);
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
                                 make_shader<"${AHO_HOME}/built-in-resource/shaders/fc.frag.spv">(device)});
#ifdef _MSC_VER
        pl::ShaderGroup input_texture_shaders("input_texture",
            { make_shader<"shaders/texture.vert.spv">(device),
             make_shader<"shaders/texture.frag.spv">(device) }),
            push_texture_shaders("push_texture",
                { make_shader<"shaders/push_texture.vert.spv">(device),
                 make_shader<"shaders/push_texture.frag.spv">(device) });
#elif defined(__APPLE_CC__)
        pl::ShaderGroup input_texture_shaders("input_texture",
                                              {make_shader<"../../AHO/shaders/texture.vert.spv">(device),
                                               make_shader<"../../AHO/shaders/texture.frag.spv">(device)}),
                push_texture_shaders("push_texture",
                                     {make_shader<"../../AHO/shaders/push_texture.vert.spv">(device),
                                      make_shader<"../../AHO/shaders/push_texture.frag.spv">(device)});
#endif

        // utils::SPIRVReflector reflector(device, std::filesystem::path(expand_environments("${AHO_HOME}/built-in-resource/shaders/all-types.vert.spv")));

        Scissor scissor(swapchain);
        Viewport viewport(swapchain);

        PipelineLayout layout(device,
                              pl::ColorBlend(),
                              pl::InputAssembly(),
                              pl::Multisample(),
                              pl::Rasterization(),
                              pl::DepthStencil(),
                              pl::DynamicState(),
                              scissor,
                              viewport);


        vsl::graphic_resource::BindingLayout ubo_binding_layout(device, {
                aho::pipeline::getBindingPoint(aho::pipeline::ResourceName::VPMatrixUBO)
        });
        auto [ubo_pool, ubo_resource] = graphic_resource_manager->allocate(
                std::vector(swapchain.getSwapImageSize(), ubo_binding_layout));

        vsl::graphic_resource::BindingLayout texture_binding_layout(device, {
                aho::pipeline::getBindingPoint(aho::pipeline::ResourceName::Texture)
        });
        auto texture_pool = graphic_resource_manager->make(
                std::map < vsl::graphic_resource::Type, size_t > {
                        {vsl::graphic_resource::Type::CombinedImageSampler, 100}
                });

        auto [input_vertices_resource_layout, input_vertices] = [&]() {
            auto vd2p_fc_umpv_reflect
                    = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments(
                                    "${AHO_HOME}/built-in-resource/shaders/vd2p_fc_umpv.vert.spv"))).generated;
            auto input_vertices_resource_layout = layout.copy().add(
                    pl::ResourceBinding(vd2p_fc_umpv_reflect.makeBindingLayout()),
                    input_d3_shaders, *vd2p_fc_umpv_reflect.vertex_input);
            return std::make_tuple(input_vertices_resource_layout,
                                   GraphicsPipeline(
                                           input_vertices_resource_layout,
                                           render_pass));
        }();

        auto [push_triangle_layout, push_triangle] = [&]() {
            auto d2triangle_single_color
                    = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments(
                                    "${AHO_HOME}/built-in-resource/shaders/2dtriangle_single_color.vert.spv")
                    )).generated;
            auto push_triangle_layout = layout.copy().add(
                    *d2triangle_single_color.push_constants,
                    push_d2_shaders);
            return std::make_tuple(push_triangle_layout,
                                   GraphicsPipeline(push_triangle_layout, render_pass));
        }();

        auto [push_texture_layout, push_texture] = [&]() {
#ifndef _MSC_VER
            auto push_texture_vert
                    = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments("../../AHO/shaders/push_texture.vert.spv"))).generated;
            auto push_texture_frag
                    = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments("../../AHO/shaders/push_texture.frag.spv"))).generated;
#else
            auto push_texture_vert
                = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                    expand_environments("shaders/push_texture.vert.spv"))).generated;
            auto push_texture_frag
                = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                    expand_environments("shaders/push_texture.frag.spv"))).generated;
#endif
            const auto push_texture_layout = layout.copy().add(
                    *push_texture_vert.push_constants,
                    pl::ResourceBinding(push_texture_frag.makeBindingLayout()),
                    push_texture_shaders);
            return std::make_tuple(push_texture_layout,
                                   GraphicsPipeline(push_texture_layout, render_pass));
        }();

        auto [texture_resource_layout, input_texture] = [&]() {
#ifndef _MSC_VER
            auto texture_vert
                    = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments("../../AHO/shaders/texture.vert.spv"))).generated;
            auto texture_frag
                    = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                            expand_environments("../../AHO/shaders/texture.frag.spv"))).generated;
#else
            auto texture_vert
                = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                    expand_environments("shaders/texture.vert.spv"))).generated;
            auto texture_frag
                = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                    expand_environments("shaders/texture.frag.spv"))).generated;
#endif
            const auto vert_layout = texture_vert.makeBindingLayout()[0];
            const auto frag_layout = texture_frag.makeBindingLayout()[0];
            std::vector layouts = {vert_layout,
                                   frag_layout};
            return std::make_tuple(layouts,
                                   GraphicsPipeline(
                                           layout.copy().add(
                                                   pl::ResourceBinding(layouts),
                                                   input_texture_shaders, *texture_vert.vertex_input),
                                           render_pass));
        }();

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
                d2::PointF{-0.5f, -0.5f},
                d2::PointF{-0.5f, 0.5f},
                d2::PointF{0.5f, 0.5f},
                d2::PointF{0.5f, -0.5f},
        };

        std::array<d2::PointF, 4> texcoords = {
                d2::PointF{0.0f, 0.0f},
                d2::PointF{0.0f, 1.0f},
                d2::PointF{1.0f, 1.0f},
                d2::PointF{1.0f, 0.0f},
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
        {
            auto size = swapchain.getSwapImageSize();
            uboBuffers.reserve(size);
            for (size_t i = 0; i < size; i++)
                uboBuffers.emplace_back(device, sizeof(ubo_t));
        }

        for (size_t i = 0; i < ubo_resource.size() && i < uboBuffers.size(); i++) {
            ubo_resource[i].update(uboBuffers[i], (size_t)
                    0, vsl::graphic_resource::Type::UniformBuffer);
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
                auto phase = command_manager.startPhase<ComputePhase>(std::nullopt, std::nullopt, in_flight);
                phase << command::ChangeImageBarrier(image, ImageLayout::TransferDstOptimal);
                phase << command::CopyBufferToImage(image, &imageStagingBuffer, ImageLayout::TransferDstOptimal);
                phase << command::ChangeImageBarrier(image, ImageLayout::TransferDstOptimal,
                                                     ImageLayout::ShaderReadOnlyOptimal);
            }
            in_flight.wait();
        }
        auto [ok, ahaha_image] = texture_pool.bind(texture_binding_layout);
        if (!ok)
            loggingln("texture_pool.bind(texture_binding_layout) failed.");
        ahaha_image.update(image, sampler, 0, vsl::graphic_resource::Type::CombinedImageSampler);

        Buffer<vsl::MemoryType::StorageBuffer,
                vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>
                debugBuffer(device, sizeof(debug_t));

        InputManager input_manager(main_window);
        auto [keyUp,
                keyDown,
                keyLeft,
                keyRight,
                keyR,
                keyF,
                keyMode,
                keyRestart] = input_manager.get<input::Keys>(
                input::KeyCode::Up,
                input::KeyCode::Down,
                input::KeyCode::Left,
                input::KeyCode::Right,
                input::KeyCode::R,
                input::KeyCode::F,
                input::KeyCode::I,
                input::KeyCode::Enter)->keys;
        auto mouse = input_manager.get<input::Mouse>();

        main_window.add_plugin<window::WindowResizeHookPlugin>([&ubo, &scissor, &viewport, &swapchain](auto w) {
            auto size = w->frame_size();
            ubo.proj = matrix::make_perspective(45.0_deg,
                                                (float)size.value.x.value / size.value.y.value,
                                                0.1f,
                                                10.0f);
            viewport = Viewport(swapchain);
            scissor = Scissor(swapchain);
        });

        vsl::IDPickingRenderPass picking_render_pass(swapchain);
        vsl::FrameBuffer picking_frame_buffer(swapchain, picking_render_pass);

        d3::VectorF move;
        int mode = 0;
        while (aho::Window::Update() && main_window && input_manager) {
            float delta = 0;
            {
                static auto prev = std::chrono::steady_clock::now();
                auto current = std::chrono::steady_clock::now();
                delta = std::chrono::duration<float>(current - prev).count();
                prev = current;
                loggingln(delta);
            }

            if (keyRestart->down())
                move = d3::VectorF();
            if (keyMode->down()) {
                mode++;
                mode %= 2;
            }
            if (keyUp->pressed())
                move.value -= 0.3_f_y * Y(delta);
            if (keyDown->pressed())
                move.value += 0.3_f_y * Y(delta);
            if (keyLeft->pressed())
                move.value -= 0.3_f_x * X(delta);
            if (keyRight->pressed())
                move.value += 0.3_f_x * X(delta);
            if (keyF->pressed())
                move.value -= 0.3_f_z * Z(delta);
            if (keyR->pressed())
                move.value += 0.3_f_z * Z(delta);


            ubo.model = matrix::make_identity<Mat4x4F>();
            ubo.view = matrix::make_view(d3::PointF(0.0f, 2.0f, 2.0f) + move, d3::PointF(0.0f, 0.0f, 0.0f) + move, d3::VectorF(0.0f, 0.0f, 1.0f));
            ubo.proj = matrix::make_perspective(45.0_deg, viewport.width / (float) viewport.height, 0.1f, 10.0f);

            {
                auto phase = DrawPhase(&engine,[&](auto& s){
                    s << command::IDPickingRenderPassBegin(picking_render_pass, picking_frame_buffer);
                });
                uboBuffers[phase.getImageIndex()].copy(ubo);

                // ubo_resource[phase.getImageIndex()].update(uboBuffers[phase.getImageIndex()], 0);
                /*
                phase << input_vertices << scissor << viewport
                      << command::BindGraphicResource(ubo_resource[phase.getImageIndex()],
                                                      graphic_resource::BindingDestination::Graphics, input_vertices)
                      << command::BindVertexBuffer(vertBuffer, colorBuffer)
                      << command::BindIndexBuffer(indexBuffer)
                      << command::DrawIndexed(indices.size());
                      */
                /*
                struct alignas(16) {
                    std::array<d2::PointF, 4> vertices;
                    std::array<d2::PointF, 4> texcoords;
                } pos_with_tc{
                        {
                                d2::PointF{-0.5f, -0.5f},
                                d2::PointF{-0.5f, 0.5f},
                                d2::PointF{0.5f, 0.5f},
                                d2::PointF{0.5f, -0.5f},
                        },
                        texcoords
                };
                static_assert(vsl::command::is_pipeline_require<vsl::command::BindGraphicResource>, "aa");
                phase << push_texture << scissor << viewport
                      << command::PushConstant(push_texture_layout, ShaderFlag::Vertex,
                                               sizeof pos_with_tc, 0, &pos_with_tc)
                      << command::BindGraphicResource(ahaha_image,
                                                      graphic_resource::BindingDestination::Graphics)
                      << command::BindIndexBuffer(indexBuffer)
                      << command::DrawIndexed(indices.size());

                /*
                                Triangle<float, vsl::D2> triangle{Point(-0.5f, 0.0f), Point(0.5f, 0.5f), Point(-0.5f, -0.5f)};
                                RGB rgb{1.0f, 0.0f, 0.0f};
                                phase << push_triangle
                                      << command::PushConstant(push_triangle_layout, ShaderFlag::Vertex, sizeof triangle, 0, &triangle)
                                      << command::PushConstant(push_triangle_layout, ShaderFlag::Vertex, sizeof rgb, 44 - sizeof rgb, &rgb)
                                      << command::Draw(3);

                                phase << input_texture
                                      << command::BindGraphicResource(
                                              {input_vertices_resource[phase.getImageIndex()], texture_resource[0]},
                                              graphic_resource::BindingDestination::Graphics, input_texture)
                                      << command::BindVertexBuffer(vertBuffer, colorBuffer, texCoordBuffer)
                                      << command::BindIndexBuffer(indexBuffer)
                                      << command::DrawIndexed(indices.size());
                                      */
            }
            // auto debugMapped = debugBuffer.data();
            // debug = static_cast<struct debug_t *>(debugMapped.data);
        }
        in_flight.wait();
        main_window.close();
        /**/
    }
#if !defined(DEBUG) && !defined(_DEBUG)
    catch (vsl::exceptions::VSLException &e) {
        vsl::loggingln(e.what());
        return 1;
    }
    catch (std::exception &e) {
        vsl::loggingln(e.what());
        return 1;
    }
#endif
    /**/
}
/* AHO/shaders/raw/vd2p_fc_umpv.vert AHOLi/AHO/object/Object.hpp AHOLi/AHO/resource/Audio.hpp AHOLi/AHO/resource/image.hpp AHOLi/AHO/resource/ObjectHitbox.hpp AHOLi/AHO/resource/ObjectModel.hpp VSLi/VSL/Vulkan/commands/bind_graphic_resource.cpp VSLi/VSL/Vulkan/commands/bind_graphic_resource.hpp VSLi/VSL/Vulkan/descriptor.cpp VSLi/VSL/Vulkan/descriptor.hpp VSLi/VSL/Vulkan/phase.cpp VSLi/VSL/Vulkan/phase.hpp VSLi/VSL/Vulkan/stages/resource_binding.cpp VSLi/VSL/Vulkan/stages/resource_binding.h
 */
// #endif