//
// Created by morimoto_hibiki on 2025/10/10.
//

#include "StandardEngine.h"

aho::engine::StandardEngine::StandardEngine(std::string applicationName, bool bootOn) {
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

    VSL_NAMESPACE::Window window(applicationName, 400, 600);
    auto surface = window.addPlugin<Surface>(instance);

    LogicalDevice device(physical_device, surface);
    vsl::loggingln("selected : ", physical_device.name(), "(", physical_device.apiVersion(), ")");

    SynchroManager synchro_manager(device);
    CommandManager command_manager(device);
    std::shared_ptr < graphic_resource::OverAllocateManager >
    graphic_resource_manager(new graphic_resource::OverAllocateManager(device, {
            {graphic_resource::Type::Sampler,              100},
            {graphic_resource::Type::CombinedImageSampler, 100},
            {graphic_resource::Type::SampledImage,         100},
            {graphic_resource::Type::StorageImage,         100},
            {graphic_resource::Type::UniformTexelBuffer,   100},
            {graphic_resource::Type::StorageTexelBuffer,   100},
            {graphic_resource::Type::UniformBuffer,        100},
            {graphic_resource::Type::StorageBuffer,        100},
    }, 0));

    _data = std::make_shared<EngineData>(EngineData{
            .vulkan_instance = instance,
            .physical_device = physical_device,
            .logical_device = device,
            .command_manager = command_manager,
            .graphic_resource_manager = graphic_resource_manager,
            .synchro_manager = synchro_manager
    });


    VSL_NAMESPACE::Swapchain swapchain(device, surface);
    VSL_NAMESPACE::View<VSL_NAMESPACE::D2> image_view(swapchain);
    VSL_NAMESPACE::RenderPass render_pass(swapchain);

    boot_window.emplace(window._data,
                        std::make_shared<AHO_NAMESPACE::window::WindowData>(AHO_NAMESPACE::window::WindowData{
                                .surface = surface,
                                .swapchain = swapchain,
                                .image_view = image_view,
                                .render_pass = render_pass,
                                .frame_buffer = VSL_NAMESPACE::FrameBuffer<VSL_NAMESPACE::D2>(swapchain,
                                                                                              image_view,
                                                                                              render_pass)
                        }));
}

std::optional<aho::window::Window> aho::engine::StandardEngine::supportWindow() {
    return boot_window;
}
