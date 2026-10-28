//
// Created by morimoto_hibiki on 2025/10/07.
//

#ifndef AHO_ALL_ENGINE_HPP
#define AHO_ALL_ENGINE_HPP

#include "define.hpp"

#include <VSL/vulkan/vulkan.hpp>
#include <VSL/vulkan/device.hpp>
#include <VSL/vulkan/command.hpp>
#include <VSL/vulkan/descriptor_set_manager.hpp>
#include <VSL/vulkan/synchronize.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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
        static EngineAccessor* INSTANCE;

    public:
        static EngineAccessor* Instance();

        EngineAccessor();
        ~EngineAccessor();

        EngineAccessor(const EngineAccessor&) = default;
        EngineAccessor& operator=(const EngineAccessor&) = default;
        EngineAccessor(EngineAccessor&&) noexcept = default;
        EngineAccessor& operator=(EngineAccessor&&) noexcept = default;

        std::shared_ptr<EngineData> data_;
    };
} // namespace AHO_NAMESPACE::engine

#endif //AHO_ALL_ENGINE_HPP
