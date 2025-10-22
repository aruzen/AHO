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
#include <VSL/Vulkan/commands/dispatch.hpp>
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
#include <VSL/Vulkan/image.hpp>
#include <VSL/Vulkan/commands/change_image_barrier.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb_image_write.h"

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
        Vulkan vk("test", { "VK_KHR_win32_surface", "VK_KHR_surface" });
#elifdef __APPLE_CC__
        Vulkan vk("test", {"VK_KHR_portability_enumeration", "VK_KHR_surface",
                           "VK_EXT_metal_surface"});
#endif
        auto physical_device = PhysicalDevices(vk).search();

        Window main_window("vsl", 800, 700);
        auto surface = main_window.addPlugin<Surface>(vk);

        LogicalDevice device(physical_device, surface);
        for (auto device: PhysicalDevices(vk).get())
            loggingln(physical_device.name(), "(", physical_device.apiVersion(), ")");
        vsl::loggingln("selected : ", physical_device.name(), "(", physical_device.apiVersion(), ")");

        SynchroManager synchro_manager(device);
#ifdef _MSC_VER
        pl::ShaderGroup fractal_shaders("fractal",
                                        {make_shader<"shaders/fractal.comp.spv">(device)});
#elifdef __APPLE_CC__
        pl::ShaderGroup fractal_shaders("fractal",
                                        {make_shader<"../../Fractal/shaders/fractal.comp.spv">(device)});
#endif
        CommandManager manager(device);
        manager.setDefault();

        GraphicResourceManager resourceManager(device);

        auto resourceBindingLayout = ResourceBindingLayout(device, {
                ResourceBindingPoint(0, ResourceType::StorageBuffer, ShaderType::Compute),
                ResourceBindingPoint(1, ResourceType::StorageBuffer, ShaderType::Compute),
                ResourceBindingPoint(2, ResourceType::UniformBuffer, ShaderType::Compute),
                ResourceBindingPoint(3, ResourceType::StorageImage, ShaderType::Compute),
                ResourceBindingPoint(4, ResourceType::StorageBuffer, ShaderType::Compute),
        });
        auto [pool, resource] = resourceManager.make({resourceBindingLayout});

        struct alignas(16) ubo_t {
            std::uint32_t baseNumber;
            std::uint32_t rowIndex;
        } ubo;

        struct alignas(16) debug_t {
            float firstColumn;
            float secondColumn;
        };

        using StorageBuffer = Buffer<vsl::MemoryType::StorageBuffer, vsl::MemoryProperty::DeviceLocal>;
        using UboBuffer = Buffer<vsl::MemoryType::UniformBuffer,
                vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
        using DebugBuffer = Buffer<vsl::MemoryType::StorageBuffer,
                vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent>;
        StorageBuffer prevRow(device, sizeof(std::uint32_t) * 1024);
        StorageBuffer currRow(device, sizeof(std::uint32_t) * 1024);
        UboBuffer uboBuffer(device, sizeof(ubo_t));
        Image fractalImage(device, 1024, 1024);
        DebugBuffer debugBuffer(device, sizeof(debug_t));

        resource[0].update(prevRow, 0);
        resource[0].update(currRow, 1);
        resource[0].update(uboBuffer, 2);
        resource[0].update(fractalImage, 3);
        resource[0].update(debugBuffer, 4);

        Buffer<vsl::MemoryType::TransferDestination,
                vsl::MemoryProperty::HostVisible | vsl::MemoryProperty::HostCoherent >
                                                   imageStagingBuffer(device, 1024 * 1024 * 4);

        PipelineLayout fractal_compute_layout(device, pl::ResourceBinding({resourceBindingLayout}), fractal_shaders);
        ComputePipeline fractal_compute(fractal_compute_layout);


        auto inFlight = synchro_manager.createFence("inFlight", manager.getBuffer().getSize(), true),
             inCopy = synchro_manager.createFence("inCopy", 1, true);

        ubo.baseNumber = 4;
        for (size_t i = 0; i < 1024; i++) {
            ubo.rowIndex = i;
            uboBuffer.copy(ubo);
            resource[0].update(uboBuffer, 2);

            if (i % 2 == 0) {
                resource[0].update(prevRow, 0);
                resource[0].update(currRow, 1);
            } else {
                resource[0].update(prevRow, 1);
                resource[0].update(currRow, 0);
            }
            {
                auto phase = manager.startPhase(std::nullopt, std::nullopt, inFlight);
                phase << fractal_compute
                      << command::PipelineBarrierChange(fractalImage)
                      << command::BindGraphicResource(resource[0], BindingDestination::Compute, fractal_compute)
                      << command::Dispatch(1 + i, 1, 1);
            }
            inFlight.wait();
        }
        imageStagingBuffer.copyByImage(fractalImage, inCopy);
        inCopy.wait();

        auto data = (std::uint32_t*)imageStagingBuffer.data().data;

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
    }
    catch (std::exception &e) {
        vsl::loggingln(e.what());
        return 1;
    }
    catch (vsl::exceptions::VSLException &e) {
        vsl::loggingln(e.what());
        return 1;
    }
}