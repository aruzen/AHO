#include <AHO/engine.hpp>
#include "engine/engine_access.hpp"
#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE::engine {
    struct DefaultEngineRegistration::Registration {
        EngineAccessor engine;
    };

    struct DefaultEngineRegistration::Registry {
        std::mutex mutex;
        std::vector<std::weak_ptr<Registration>> registrations;
    };

    EngineAccessor::EngineAccessor(std::shared_ptr<EngineData> data) : _data(std::move(data)) {}

    EngineAccessor EngineAccessor::Create(EngineCreateInfo info) {
        if (!info.device || !info.commands || !info.descriptor_manager || !*info.descriptor_manager)
            throw std::invalid_argument("Engine requires Device, commands and a descriptor manager");
        const auto features = info.device.enabled_features();
        if (info.commands.get_device() != info.device || info.descriptor_manager->get_device() != info.device ||
            (info.synchronization && info.synchronization.get_device() != info.device))
            throw std::invalid_argument("Engine services must use the same Device");
        if (info.present_fences != VSL_NAMESPACE::PresentFencePolicy::LEGACY &&
            info.present_fences != VSL_NAMESPACE::PresentFencePolicy::REQUIRE_FENCE)
            throw std::invalid_argument("Unknown presentation fence policy");
        if (info.present_fences == VSL_NAMESPACE::PresentFencePolicy::REQUIRE_FENCE &&
            !VSL_NAMESPACE::has_device_features(features, VSL_NAMESPACE::DeviceFeature::SWAPCHAIN_MAINTENANCE1))
            throw std::invalid_argument("Presentation fences require the enabled Device feature");
        return EngineAccessor(std::make_shared<EngineData>(EngineData{
            .logical_device = std::move(info.device),
            .command_manager = std::move(info.commands),
            .graphic_resource_manager = std::move(info.descriptor_manager),
            .synchro_manager = std::move(info.synchronization),
            .present_fences = info.present_fences}));
    }

    VSL_NAMESPACE::LogicalDeviceAccessor EngineAccessor::device() const {
        return detail::EngineAccess::Require(*this).logical_device;
    }

    VSL_NAMESPACE::PhysicalDevice EngineAccessor::physical_device() const {
        return device().get_physical_device();
    }

    VSL_NAMESPACE::VulkanAccessor EngineAccessor::instance() const {
        return physical_device().get_instance();
    }

    VSL_NAMESPACE::CommandManager EngineAccessor::commands() const {
        return detail::EngineAccess::Require(*this).command_manager;
    }

    std::shared_ptr<VSL_NAMESPACE::graphic_resource::ManagerInterface> EngineAccessor::descriptor_manager() const {
        return detail::EngineAccess::Require(*this).graphic_resource_manager;
    }

    VSL_NAMESPACE::SynchroManager EngineAccessor::synchronization() const {
        return detail::EngineAccess::Require(*this).synchro_manager;
    }

    VSL_NAMESPACE::PresentFencePolicy EngineAccessor::present_fences() const {
        return detail::EngineAccess::Require(*this).present_fences;
    }

    DefaultEngineRegistration::Registry& DefaultEngineRegistration::State() {
        static Registry REGISTRY;
        return REGISTRY;
    }

    DefaultEngineRegistration::DefaultEngineRegistration(EngineAccessor engine) {
        if (!engine)
            throw std::invalid_argument("DefaultEngineRegistration requires engine data");
        auto registration = std::make_shared<Registration>(std::move(engine));
        auto& state = State();
        std::lock_guard lock(state.mutex);
        std::erase_if(state.registrations, [](const auto& entry) { return entry.expired(); });
        state.registrations.push_back(registration);
        _registration = std::move(registration);
    }

    EngineAccessor DefaultEngineRegistration::Current() {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        std::erase_if(state.registrations, [](const auto& entry) { return entry.expired(); });
        for (auto entry = state.registrations.rbegin(); entry != state.registrations.rend(); ++entry) {
            if (const auto registration = entry->lock())
                return registration->engine;
        }
        return {};
    }
} // namespace AHO_NAMESPACE::engine
