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

    class EngineAccessor {
        std::shared_ptr<EngineData> _data;

    protected:
        void reset_data(std::shared_ptr<EngineData> data);

    public:
        EngineAccessor() = default;
        explicit EngineAccessor(std::shared_ptr<EngineData> data);
        ~EngineAccessor() = default;

        EngineAccessor(const EngineAccessor&) = default;
        EngineAccessor& operator=(const EngineAccessor&) = default;
        EngineAccessor(EngineAccessor&&) noexcept = default;
        EngineAccessor& operator=(EngineAccessor&&) noexcept = default;

        const std::shared_ptr<EngineData>& data() const;
    };

    // 登録の寿命だけ既定エンジンを保持する。新しい登録の終了後は前の登録へ戻る。
    class DefaultEngineRegistration {
        struct Registration;
        struct Registry;
        std::shared_ptr<Registration> _registration;
        static Registry& State();

    public:
        explicit DefaultEngineRegistration(EngineAccessor engine);
        ~DefaultEngineRegistration() = default;
        DefaultEngineRegistration(const DefaultEngineRegistration&) = delete;
        DefaultEngineRegistration& operator=(const DefaultEngineRegistration&) = delete;
        DefaultEngineRegistration(DefaultEngineRegistration&&) noexcept = default;
        DefaultEngineRegistration& operator=(DefaultEngineRegistration&&) noexcept = default;

        static EngineAccessor Current();
    };
} // namespace AHO_NAMESPACE::engine

#endif //AHO_ALL_ENGINE_HPP
