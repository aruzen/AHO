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
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AHO_NAMESPACE::window {
    struct Window;
}

namespace AHO_NAMESPACE::engine {
    namespace detail {
        struct EngineAccess;
    }
    struct EngineData;
    struct EngineConfig {
        std::string application_name = "AHO";
        std::vector<std::string> instance_extensions;
        std::optional<std::size_t> physical_device_index;
        std::map<VSL_NAMESPACE::graphic_resource::Type, std::size_t> descriptor_budget = {
            {VSL_NAMESPACE::graphic_resource::Type::SAMPLER, 100},
            {VSL_NAMESPACE::graphic_resource::Type::COMBINED_IMAGE_SAMPLER, 100},
            {VSL_NAMESPACE::graphic_resource::Type::SAMPLED_IMAGE, 100},
            {VSL_NAMESPACE::graphic_resource::Type::STORAGE_IMAGE, 100},
            {VSL_NAMESPACE::graphic_resource::Type::UNIFORM_TEXEL_BUFFER, 100},
            {VSL_NAMESPACE::graphic_resource::Type::STORAGE_TEXEL_BUFFER, 100},
            {VSL_NAMESPACE::graphic_resource::Type::UNIFORM_BUFFER, 100},
            {VSL_NAMESPACE::graphic_resource::Type::STORAGE_BUFFER, 100}};
        // 起動時の能力要求と、生成するWindowの既定policyを明示する。
        VSL_NAMESPACE::PresentFencePolicy present_fences = VSL_NAMESPACE::PresentFencePolicy::LEGACY;
    };

    struct EngineCreateInfo {
        VSL_NAMESPACE::LogicalDeviceAccessor device;
        VSL_NAMESPACE::CommandManager commands;
        std::shared_ptr<VSL_NAMESPACE::graphic_resource::ManagerInterface> descriptor_manager;
        VSL_NAMESPACE::SynchroManager synchronization;
        VSL_NAMESPACE::PresentFencePolicy present_fences = VSL_NAMESPACE::PresentFencePolicy::LEGACY;
    };

    class EngineAccessor {
        std::shared_ptr<EngineData> _data;

        friend struct detail::EngineAccess;
        explicit EngineAccessor(std::shared_ptr<EngineData> data);

    public:
        EngineAccessor() = default;
        ~EngineAccessor() = default;

        EngineAccessor(const EngineAccessor&) = default;
        EngineAccessor(EngineAccessor&&) noexcept = default;
        EngineAccessor& operator=(EngineAccessor value) noexcept {
            swap(value);
            return *this;
        }
        void swap(EngineAccessor& other) noexcept { _data.swap(other._data); }
        explicit operator bool() const noexcept { return bool(_data); }
        bool operator==(const EngineAccessor&) const = default;
        static EngineAccessor Create(EngineCreateInfo info);
        VSL_NAMESPACE::LogicalDeviceAccessor device() const;
        VSL_NAMESPACE::PhysicalDevice physical_device() const;
        VSL_NAMESPACE::VulkanAccessor instance() const;
        VSL_NAMESPACE::CommandManager commands() const;
        std::shared_ptr<VSL_NAMESPACE::graphic_resource::ManagerInterface> descriptor_manager() const;
        VSL_NAMESPACE::SynchroManager synchronization() const;
        VSL_NAMESPACE::PresentFencePolicy present_fences() const;
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
