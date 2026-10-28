#include <AHO/core/math/projection.hpp>
#include <VSL/vsl.hpp>
#include <VSL/utils/shader_compiler.hpp>
#include <VSL/utils/spir_reflector.hpp>

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning(disable : 4455)

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

        auto& [vulkan_instance, physical_device, device, command_manager, graphic_resource_manager, synchro_manager] = *engine.data_;
        auto& main_window = engine.boot_window.value();
        auto& [surface, swapchain, render_pass,
               frame_buffer, image_available, render_finish, in_flight] = *engine.boot_window.value().data2;
        if (!image_available.data_)
            loggingln("image not available ; ;");
        main_window.resize(500, 500);
#ifdef _MSC_VER
        vsl::utils::ShaderCompiler shader_compiler("glslc", {"shaders/", "${AHO_HOME}/built-in-resource/shaders/"});
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
                                              {make_shader<"shaders/texture.vert.spv">(device),
                                               make_shader<"shaders/texture.frag.spv">(device)}),
            push_texture_shaders("push_texture",
                                 {make_shader<"shaders/push_texture.vert.spv">(device),
                                  make_shader<"shaders/push_texture.frag.spv">(device)});
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

        vsl::graphic_resource::BindingLayout ubo_binding_layout(device, {aho::pipeline::get_binding_point(aho::pipeline::ResourceName::VP_MATRIX_UBO)});
        auto [ubo_pool, ubo_resource] = graphic_resource_manager->allocate(
            std::vector(swapchain.get_swap_image_size(), ubo_binding_layout));

        vsl::graphic_resource::BindingLayout texture_binding_layout(device, {aho::pipeline::get_binding_point(aho::pipeline::ResourceName::TEXTURE)});
        auto texture_pool = graphic_resource_manager->make(
            std::map<vsl::graphic_resource::Type, size_t>{
                {vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, 100}});

        auto [input_vertices_resource_layout, input_vertices] = [&]() {
            auto vd2p_fc_umpv_reflect = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                               expand_environments(
                                                                                   "${AHO_HOME}/built-in-resource/shaders/vd2p_fc_umpv.vert.spv")))
                                            .generated;
            auto input_vertices_resource_layout = layout.copy().add(
                pl::ResourceBinding(vd2p_fc_umpv_reflect.make_binding_layout()),
                input_d3_shaders, *vd2p_fc_umpv_reflect.vertex_input);
            return std::make_tuple(input_vertices_resource_layout,
                                   GraphicsPipeline(
                                       input_vertices_resource_layout,
                                       render_pass));
        }();

        auto [push_triangle_layout, push_triangle] = [&]() {
            auto d2triangle_single_color = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                                  expand_environments(
                                                                                      "${AHO_HOME}/built-in-resource/shaders/2dtriangle_single_color.vert.spv")))
                                               .generated;
            auto push_triangle_layout = layout.copy().add(
                *d2triangle_single_color.push_constants,
                push_d2_shaders);
            return std::make_tuple(push_triangle_layout,
                                   GraphicsPipeline(push_triangle_layout, render_pass));
        }();

        auto [push_texture_layout, push_texture] = [&]() {
#ifndef _MSC_VER
            auto push_texture_vert = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                            expand_environments("../../AHO/shaders/push_texture.vert.spv")))
                                         .generated;
            auto push_texture_frag = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                            expand_environments("../../AHO/shaders/push_texture.frag.spv")))
                                         .generated;
#else
            auto push_texture_vert = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                            expand_environments("shaders/push_texture.vert.spv")))
                                         .generated;
            auto push_texture_frag = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                            expand_environments("shaders/push_texture.frag.spv")))
                                         .generated;
#endif
            const auto push_texture_layout = layout.copy().add(
                *push_texture_vert.push_constants,
                pl::ResourceBinding(push_texture_frag.make_binding_layout()),
                push_texture_shaders);
            return std::make_tuple(push_texture_layout,
                                   GraphicsPipeline(push_texture_layout, render_pass));
        }();

        auto [texture_resource_layout, input_texture] = [&]() {
#ifndef _MSC_VER
            auto texture_vert = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                       expand_environments("../../AHO/shaders/texture.vert.spv")))
                                    .generated;
            auto texture_frag = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                       expand_environments("../../AHO/shaders/texture.frag.spv")))
                                    .generated;
#else
            auto texture_vert = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                       expand_environments("shaders/texture.vert.spv")))
                                    .generated;
            auto texture_frag = vsl::utils::SPIRVReflector(device, std::filesystem::path(
                                                                       expand_environments("shaders/texture.frag.spv")))
                                    .generated;
#endif
            const auto vert_layout = texture_vert.make_binding_layout()[0];
            const auto frag_layout = texture_frag.make_binding_layout().at(1);
            std::vector layouts = {vert_layout,
                                   frag_layout};
            return std::make_tuple(layouts,
                                   GraphicsPipeline(
                                       layout.copy().add(
                                           pl::ResourceBinding(layouts),
                                           input_texture_shaders, *texture_vert.vertex_input),
                                       render_pass));
        }();

        struct alignas(16) UboT {
            Mat4x4<float> model;
            Mat4x4<float> view;
            Mat4x4<float> proj;
        } ubo;

        std::array<RGB, 4> colors = {
            RGB{1.0f, 0.0f, 0.0f},
            RGB{0.0f, 1.0f, 0.0f},
            RGB{0.0f, 0.0f, 1.0f},
            RGB{1.0f, 1.0f, 1.0f}};

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
            0, 1, 2, 2, 3, 0};

        struct alignas(16) DebugT {
            std::array<float, 4 * 3> v;
        }* debug;

        using UboBuffer = Buffer<vsl::MemoryType::UNIFORM_BUFFER,
                                 vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT>;
        std::vector<UboBuffer> ubo_buffers;
        {
            auto size = swapchain.get_swap_image_size();
            ubo_buffers.reserve(size);
            for (size_t i = 0; i < size; i++)
                ubo_buffers.emplace_back(device, sizeof(UboT));
        }

        for (size_t i = 0; i < ubo_resource.size() && i < ubo_buffers.size(); i++) {
            ubo_resource[i].update(ubo_buffers[i], (size_t)0, vsl::graphic_resource::Type::UNIFORM_BUFFER);
        }
        DeviceLocalBuffer<vsl::MemoryType::VERTEX_BUFFER> color_buffer(device, command_manager, colors);
        DeviceLocalBuffer<vsl::MemoryType::INDEX_BUFFER> index_buffer(device, command_manager, indices);
        DeviceLocalBuffer<vsl::MemoryType::VERTEX_BUFFER> vert_buffer(device, command_manager, vertices);
        DeviceLocalBuffer<vsl::MemoryType::VERTEX_BUFFER> tex_coord_buffer(device, command_manager, texcoords);

        int tex_width, tex_height, tex_channels;
        stbi_uc* pixels = stbi_load(expand_environments("${AHO_HOME}/built-in-resource/images/ahahacraft.png").c_str(),
                                    &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        Image<ImageType::TRANSFER_DST | ImageType::SAMPLED> image(device, tex_width, tex_height);
        Sampler sampler(device);
        {
            StagingBuffer<> image_staging_buffer(device, tex_height * tex_width * tex_channels);
            auto data = image_staging_buffer.data();
            memcpy(data.data, pixels, tex_height * tex_width * tex_channels);
            {
                auto phase = command_manager.start_phase<ComputePhase>(std::nullopt, std::nullopt, in_flight);
                phase << vsl::command::ChangeImageBarrier(image, ImageLayout::TRANSFER_DST_OPTIMAL);
                phase << vsl::command::CopyBufferToImage(image, &image_staging_buffer, ImageLayout::TRANSFER_DST_OPTIMAL);
                phase << vsl::command::ChangeImageBarrier(image, ImageLayout::TRANSFER_DST_OPTIMAL,
                                                          ImageLayout::SHADER_READ_ONLY_OPTIMAL,
                                                          aho::pipeline::get_binding_point(aho::pipeline::ResourceName::TEXTURE).shader_type);
            }
            in_flight.wait();
        }
        auto [ok, ahaha_image] = texture_pool.bind(texture_binding_layout);
        if (!ok)
            loggingln("texture_pool.bind(texture_binding_layout) failed.");
        ahaha_image.update(image, sampler, 0, vsl::graphic_resource::Type::COMBINED_IMAGE_SAMPLER);

        Buffer<vsl::MemoryType::STORAGE_BUFFER,
               vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT>
            debug_buffer(device, sizeof(DebugT));

        InputManager input_manager(main_window);
        auto [key_up,
              key_down,
              key_left,
              key_right,
              key_r,
              key_f,
              key_mode,
              key_restart] = input_manager.get<input::Keys>(input::KeyCode::UP,
                                                            input::KeyCode::DOWN,
                                                            input::KeyCode::LEFT,
                                                            input::KeyCode::RIGHT,
                                                            input::KeyCode::R,
                                                            input::KeyCode::F,
                                                            input::KeyCode::I,
                                                            input::KeyCode::ENTER)
                                 ->keys;
        auto mouse = input_manager.get<input::Mouse>();

        main_window.add_plugin<window::WindowResizeHookPlugin>([&ubo, &scissor, &viewport, &swapchain](auto w) {
            auto size = w->frame_size();
            ubo.proj = matrix::make_perspective<HandedCoordinate::RIGHT, DepthRange::NEGATIVE_ONE_TO_ONE, ClipY::UP, VectorConvention::ROW>(45.0_deg,
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
                static auto PREV = std::chrono::steady_clock::now();
                auto current = std::chrono::steady_clock::now();
                delta = std::chrono::duration<float>(current - PREV).count();
                PREV = current;
                loggingln(delta);
            }

            if (mouse->left_click()->up()) {
                auto id = picking_render_pass.read(command_manager, in_flight, 500, 500);
                loggingln("clicked : ", id);
            }

            if (key_restart->down())
                move = d3::VectorF();
            if (key_mode->down()) {
                mode++;
                mode %= 2;
            }
            if (key_up->pressed())
                move.value -= 0.3_f_y * Y(delta);
            if (key_down->pressed())
                move.value += 0.3_f_y * Y(delta);
            if (key_left->pressed())
                move.value -= 0.3_f_x * X(delta);
            if (key_right->pressed())
                move.value += 0.3_f_x * X(delta);
            if (key_f->pressed())
                move.value -= 0.3_f_z * Z(delta);
            if (key_r->pressed())
                move.value += 0.3_f_z * Z(delta);

            ubo.model = matrix::make_identity<Mat4x4F>();
            ubo.view = matrix::make_view<HandedCoordinate::RIGHT, ViewOrientation::ROLL_180, VectorConvention::ROW>(d3::PointF(0.0f, 2.0f, 2.0f) + move, d3::PointF(0.0f, 0.0f, 0.0f) + move, d3::VectorF(0.0f, 0.0f, 1.0f));
            ubo.proj = matrix::make_perspective<HandedCoordinate::RIGHT, DepthRange::NEGATIVE_ONE_TO_ONE, ClipY::UP, VectorConvention::ROW>(45.0_deg, viewport.width / (float)viewport.height, 0.1f, 10.0f);

            {
                auto phase = DrawPhase(&engine);
                ubo_buffers[phase.get_current_index()].copy(ubo);

                //ubo_resource[phase.get_current_index()].update(ubo_buffers[phase.get_current_index()], 0);

                phase << input_vertices << scissor << viewport
                      << vsl::command::BindGraphicResource(ubo_resource[phase.get_image_index()],
                                                           graphic_resource::BindingDestination::GRAPHICS, input_vertices)
                      << vsl::command::BindVertexBuffer(vert_buffer, color_buffer)
                      << vsl::command::BindIndexBuffer(index_buffer)
                      << vsl::command::DrawIndexed(indices.size());

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
                      << command::PushConstant(push_texture_layout, ShaderFlag::VERTEX,
                                               sizeof pos_with_tc, 0, &pos_with_tc)
                      << command::BindGraphicResource(ahaha_image,
                                                      graphic_resource::BindingDestination::GRAPHICS)
                      << command::BindIndexBuffer(index_buffer)
                      << command::DrawIndexed(indices.size());

                /*
                                Triangle<float, vsl::D2> triangle{Point(-0.5f, 0.0f), Point(0.5f, 0.5f), Point(-0.5f, -0.5f)};
                                RGB rgb{1.0f, 0.0f, 0.0f};
                                phase << push_triangle
                                      << command::PushConstant(push_triangle_layout, ShaderFlag::VERTEX, sizeof triangle, 0, &triangle)
                                      << command::PushConstant(push_triangle_layout, ShaderFlag::VERTEX, sizeof rgb, 44 - sizeof rgb, &rgb)
                                      << command::Draw(3);

                                phase << input_texture
                                      << command::BindGraphicResource(
                                              {input_vertices_resource[phase.get_image_index()], texture_resource[0]},
                                              graphic_resource::BindingDestination::GRAPHICS, input_texture)
                                      << command::BindVertexBuffer(vert_buffer, color_buffer, tex_coord_buffer)
                                      << command::BindIndexBuffer(index_buffer)
                                      << command::DrawIndexed(indices.size());
                                      */
            }
            // auto debug_mapped = debug_buffer.data();
            // debug = static_cast<struct DebugT *>(debug_mapped.data);
        }
        in_flight.wait();
        main_window.close();
        /**/
    }
#if !defined(DEBUG) && !defined(_DEBUG)
    catch (vsl::exceptions::VSLException& e) {
        vsl::loggingln(e.what());
        return 1;
    } catch (std::exception& e) {
        vsl::loggingln(e.what());
        return 1;
    }
#endif
    /**/
}
/* AHO/shaders/raw/vd2p_fc_umpv.vert AHOLi/AHO/object/Object.hpp AHOLi/AHO/resource/Audio.hpp AHOLi/AHO/resource/image.hpp AHOLi/AHO/resource/ObjectHitbox.hpp AHOLi/AHO/resource/ObjectModel.hpp VSLi/VSL/Vulkan/commands/bind_graphic_resource.cpp VSLi/VSL/Vulkan/commands/bind_graphic_resource.hpp VSLi/VSL/Vulkan/descriptor.cpp VSLi/VSL/Vulkan/descriptor.hpp VSLi/VSL/Vulkan/phase.cpp VSLi/VSL/Vulkan/phase.hpp VSLi/VSL/Vulkan/stages/resource_binding.cpp VSLi/VSL/Vulkan/stages/resource_binding.h
 */
// #endif
