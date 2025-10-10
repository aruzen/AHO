//
// Created by morimoto_hibiki on 2025/10/07.
//

#include "Engine.hpp"

#include <memory>

aho::engine::EngineAccessor* aho::engine::EngineAccessor::_instance = nullptr;

aho::engine::EngineAccessor *aho::engine::EngineAccessor::instance() {
    return _instance;
}

aho::engine::EngineAccessor::EngineAccessor() {
    if (_instance == nullptr)
        _instance = this;
}

aho::engine::StandardEngine::StandardEngine(std::string applicationName, bool bootOn) : boot_window("boot...", 800, 800) {
    if (bootOn)
        boot(applicationName);
}

void aho::engine::StandardEngine::boot(std::string applicationName) {
    using namespace VSL_NAMESPACE;
#ifdef _MSC_VER
    Vulkan<> instance(applicationName.c_str(), { "VK_KHR_win32_surface", "VK_KHR_surface" });
#elifdef __APPLE_CC__
    Vulkan<> instance(applicationName.c_str(), {"VK_KHR_portability_enumeration", "VK_KHR_surface",
                                                               "VK_EXT_metal_surface"});
#endif
    auto physical_device = PhysicalDevices(instance).search();

    surface = boot_window.addPlugin<Surface>(instance);

    LogicalDevice device(physical_device, surface);
    vsl::loggingln("selected : ", physical_device.name(), "(", physical_device.apiVersion(), ")");

    SynchroManager synchro_manager(device);
    CommandManager command_manager(device);
    GraphicResourceManager graphic_resource_manager(device);

    _data = std::make_shared<EngineData>(EngineData{
        .vulkan_instance = instance,
        .physical_device = physical_device,
        .logical_device = device,
        .command_manager = command_manager,
        .graphic_resource_manager = graphic_resource_manager,
        .synchro_manager = synchro_manager
    });
}

std::optional<vsl::Window> aho::engine::StandardEngine::supportWindow() {
    return boot_window;
}

std::optional<std::shared_ptr<vsl::Surface>> aho::engine::StandardEngine::supportSurface() {
    return surface;
}
