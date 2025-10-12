//
// Created by morimoto_hibiki on 2025/10/07.
//

#ifndef AHO_ALL_ENGINE_HPP
#define AHO_ALL_ENGINE_HPP

#include "define.h"

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
#include <VSL/Vulkan/descriptor.hpp>
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

namespace AHO_NAMESPACE::window {
    struct Window;
}

namespace AHO_NAMESPACE::engine {
    struct EnginePrespecification {
        std::string name;
        std::vector<std::filesystem::path> shaders_directories;
    };

    struct EngineData {
        VSL_NAMESPACE::Vulkan<> vulkan_instance;
        VSL_NAMESPACE::PhysicalDevice physical_device;
        VSL_NAMESPACE::LogicalDeviceAccessor logical_device;
        VSL_NAMESPACE::CommandManager command_manager;
        VSL_NAMESPACE::GraphicResourceManager graphic_resource_manager;
        VSL_NAMESPACE::SynchroManager synchro_manager;
    };

    struct EngineAccessor {
    private:
        static EngineAccessor *_instance;
    public:
        static EngineAccessor *instance();

        EngineAccessor();

        std::shared_ptr<EngineData> _data;

        virtual std::optional<AHO_NAMESPACE::window::Window> supportWindow();
    };
}


#endif //AHO_ALL_ENGINE_HPP
