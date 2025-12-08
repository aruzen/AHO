//
// Created by morimoto_hibiki on 2025/10/07.
//

#ifndef AHO_ALL_ENGINE_HPP
#define AHO_ALL_ENGINE_HPP

#include "define.h"

#define VSL_ENABLE_SPIRV_REFLECTOR
#include <VSL/vsl.h>

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
        std::shared_ptr<VSL_NAMESPACE::graphic_resource::ManagerInterface> graphic_resource_manager;
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
