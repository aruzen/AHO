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
#pragma warning( disable : 4455 )

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