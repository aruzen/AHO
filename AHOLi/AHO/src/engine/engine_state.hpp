#pragma once
#include <AHO/engine.hpp>

namespace AHO_NAMESPACE::engine {
    struct EngineData {
        VSL_NAMESPACE::LogicalDeviceAccessor logical_device;
        VSL_NAMESPACE::CommandManager command_manager;
        std::shared_ptr<VSL_NAMESPACE::graphic_resource::ManagerInterface> graphic_resource_manager;
        VSL_NAMESPACE::SynchroManager synchro_manager;
        VSL_NAMESPACE::PresentFencePolicy present_fences = VSL_NAMESPACE::PresentFencePolicy::LEGACY;
    };
} // namespace AHO_NAMESPACE::engine
