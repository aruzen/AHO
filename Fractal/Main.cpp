#include <VSL/concepts.hpp>
#include "VSL/utils/string.hpp"
#include <VSL/define.hpp>
#include <VSL/dimension.hpp>
#include <VSL/exceptions.hpp>
#include <VSL/test.hpp>
#include <VSL/type.hpp>
#include <VSL/Window.hpp>
#include <VSL/window_plugin.hpp>
#include <VSL/utils/ShaderCompiler.hpp>
#include <VSL/utils/VSLArray.hpp>
#include <VSL/vulkan/buffer.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/device.hpp>
#include <VSL/vulkan/frame_buffer.hpp>
#include <VSL/vulkan/pipeline.hpp>
#include <VSL/vulkan/pipeline_layout.hpp>
#include <VSL/vulkan/pv.hpp>
#include <VSL/vulkan/phase.hpp>
#include <VSL/vulkan/render_pass.hpp>
#include <VSL/vulkan/scissor.hpp>
#include <VSL/vulkan/shader.hpp>
#include <VSL/vulkan/surface.hpp>
#include <VSL/vulkan/swapchain.hpp>
#include <VSL/vulkan/synchronize.hpp>
#include <VSL/vulkan/view.hpp>
#include <VSL/vulkan/viewport.hpp>
#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/commands/bind_pipe_line.hpp>
#include <VSL/vulkan/commands/bind_vertex_buffers.hpp>
#include <VSL/vulkan/commands/bind_index_buffers.hpp>
#include <VSL/vulkan/commands/bind_graphic_resource.hpp>
#include <VSL/vulkan/commands/draw.hpp>
#include <VSL/vulkan/commands/draw_indexed.hpp>
#include <VSL/vulkan/commands/dispatch.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <VSL/vulkan/stages/color_blend.hpp>
#include <VSL/vulkan/stages/depth_stencil.hpp>
#include <VSL/vulkan/stages/dynamic_state.hpp>
#include <VSL/vulkan/stages/input_assembly.hpp>
#include <VSL/vulkan/stages/multisample.hpp>
#include <VSL/vulkan/stages/rasterization.hpp>
#include <VSL/vulkan/stages/resource_binding.hpp>
#include <VSL/vulkan/stages/shader_group.hpp>
#include <VSL/vulkan/stages/vertex_input.hpp>
#include <VSL/vulkan/descriptor.hpp>
#include <VSL/vulkan/image.hpp>
#include <VSL/vulkan/commands/change_image_barrier.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb_image_write.h"

// #define AHO_POOP_PUBLIC_SECURITY
#pragma warning(disable : 4455)

#include <AHO/define.hpp>
#include <AHO/core/math/Mat.hpp>
#include <AHO/core/math/coordinate.hpp>
#include <AHO/core/math/angle.hpp>

#include <AHO/core/Vector.hpp>
#include <AHO/core/Point.hpp>
#include <AHO/core/Triangle.hpp>
#include <AHO/core/Polygon.hpp>
#include <AHO/core/color.hpp>

#include <AHO/io/Key.hpp>
#include <AHO/io/KeyBoard.hpp>

#include <chrono>

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
        vsl::utils::ShaderCompiler shader_compiler("glslc", "../../Fractal/shaders/");
#endif
        shader_compiler.load();
        shader_compiler.compile();

#ifdef _MSC_VER
        Vulkan vk("test", {"VK_KHR_win32_surface", "VK_KHR_surface"});
#elifdef __APPLE_CC__
        Vulkan vk("test", {"VK_KHR_portability_enumeration", "VK_KHR_surface",
                           "VK_EXT_metal_surface"});
#endif
        auto physical_device = PhysicalDevices(vk).search();

        Window main_window("vsl", 800, 700);
        auto surface = main_window.add_plugin<Surface>(vk);

        LogicalDevice device(physical_device, surface);
        for (auto device : PhysicalDevices(vk).get())
            loggingln(physical_device.name(), "(", physical_device.api_version(), ")");
        vsl::loggingln("selected : ", physical_device.name(), "(", physical_device.api_version(), ")");

        SynchroManager synchro_manager(device);
#ifdef _MSC_VER
        pl::ShaderGroup fractal_shaders("fractal",
                                        {make_shader<"shaders/fractal.comp.spv">(device)});
#elifdef __APPLE_CC__
        pl::ShaderGroup fractal_shaders("fractal",
                                        {make_shader<"../../Fractal/shaders/fractal.comp.spv">(device)});
#endif
        CommandManager manager(device);
        manager.set_default();

        GraphicResourceManager resource_manager(device);

        auto resource_binding_layout = ResourceBindingLayout(device, {
                                                                       ResourceBindingPoint(0, ResourceType::STORAGE_BUFFER, ShaderFlag::COMPUTE),
                                                                       ResourceBindingPoint(1, ResourceType::STORAGE_BUFFER, ShaderFlag::COMPUTE),
                                                                       ResourceBindingPoint(2, ResourceType::UNIFORM_BUFFER, ShaderFlag::COMPUTE),
                                                                       ResourceBindingPoint(3, ResourceType::STORAGE_IMAGE, ShaderFlag::COMPUTE),
                                                                       ResourceBindingPoint(4, ResourceType::STORAGE_BUFFER, ShaderFlag::COMPUTE),
                                                                   });
        auto [pool, resource] = resource_manager.make({resource_binding_layout});

        struct alignas(16) UniformData {
            std::uint32_t base_number;
            std::uint32_t row_index;
        } ubo;

        struct alignas(16) DebugData {
            float first_column;
            float second_column;
        };

        using StorageBuffer = Buffer<vsl::MemoryType::STORAGE_BUFFER, vsl::MemoryProperty::DEVICE_LOCAL>;
        using UniformBuffer = Buffer<vsl::MemoryType::UNIFORM_BUFFER,
                                 vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT>;
        using DebugBuffer = Buffer<vsl::MemoryType::STORAGE_BUFFER,
                                   vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT>;
        StorageBuffer prev_row(device, sizeof(std::uint32_t) * 1024);
        StorageBuffer curr_row(device, sizeof(std::uint32_t) * 1024);
        UniformBuffer ubo_buffer(device, sizeof(UniformData));
        Image fractal_image(device, 1024, 1024);
        DebugBuffer debug_buffer(device, sizeof(DebugData));

        resource[0].update(prev_row, 0);
        resource[0].update(curr_row, 1);
        resource[0].update(ubo_buffer, 2);
        resource[0].update(fractal_image, 3);
        resource[0].update(debug_buffer, 4);

        Buffer<vsl::MemoryType::TRANSFER_DESTINATION,
               vsl::MemoryProperty::HOST_VISIBLE | vsl::MemoryProperty::HOST_COHERENT>
            image_staging_buffer(device, 1024 * 1024 * 4);

        PipelineLayout fractal_compute_layout(device, pl::ResourceBinding({resource_binding_layout}), fractal_shaders);
        ComputePipeline fractal_compute(fractal_compute_layout);

        auto in_flight = synchro_manager.create_fence("inFlight", manager.get_buffer().get_size(), true),
             in_copy = synchro_manager.create_fence("inCopy", 1, true);

        ubo.base_number = 4;
        for (size_t i = 0; i < 1024; i++) {
            ubo.row_index = i;
            ubo_buffer.copy(ubo);
            resource[0].update(ubo_buffer, 2);

            if (i % 2 == 0) {
                resource[0].update(prev_row, 0);
                resource[0].update(curr_row, 1);
            } else {
                resource[0].update(prev_row, 1);
                resource[0].update(curr_row, 0);
            }
            {
                auto phase = manager.start_phase<ComputePhase>(std::nullopt, std::nullopt, in_flight);
                phase << fractal_compute
                      << command::PipelineBarrierChange(fractal_image)
                      << command::BindGraphicResource(resource[0], BindingDestination::COMPUTE, fractal_compute)
                      << command::Dispatch(1 + i, 1, 1);
            }
            in_flight.wait();
        }
        image_staging_buffer.copy_by_image(fractal_image, in_copy);
        in_copy.wait();

        auto data = (std::uint32_t*)image_staging_buffer.data().data;

        int result = stbi_write_png("output.png",
                                    1024,
                                    1024,
                                    4,
                                    data,
                                    1024 * 4);
        if (!result) {
            throw std::runtime_error("Failed to write PNG file!");
        }
        defaults::release();
    } catch (std::exception& e) {
        vsl::loggingln(e.what());
        return 1;
    } catch (vsl::exceptions::VSLException& e) {
        vsl::loggingln(e.what());
        return 1;
    }
}
