#include <AHO/engine/standard_engine.hpp>

#include <utility>

AHO_NAMESPACE::engine::StandardEngine::StandardEngine(std::string application_name, bool boot_on) {
    if (boot_on)
        boot(std::move(application_name));
}

void AHO_NAMESPACE::engine::StandardEngine::boot(const std::string& application_name) {
    using namespace VSL_NAMESPACE;
#ifdef _MSC_VER
    Vulkan<> instance(application_name.c_str(), {"VK_KHR_win32_surface", "VK_KHR_surface",
                                                 "VK_KHR_get_physical_device_properties2"});
#elif defined(__APPLE_CC__)
    Vulkan<> instance(application_name.c_str(), {"VK_KHR_portability_enumeration", "VK_KHR_surface",
                                                 "VK_EXT_metal_surface", "VK_KHR_get_physical_device_properties2"});
#endif
    auto physical_device = PhysicalDevices(instance).search();

    // ロゴ表示を想定し、初期ウィンドウを400×400で作成する。
    VSL_NAMESPACE::Window window(application_name, 400, 400);
    auto surface = window.add_plugin<Surface>(instance);

    LogicalDevice device(physical_device, surface);

    SynchroManager synchro_manager(device);
    CommandManager command_manager(device);
    std::shared_ptr<graphic_resource::OverAllocateManager>
        graphic_resource_manager(new graphic_resource::OverAllocateManager(device, {
                                                                                       {graphic_resource::Type::SAMPLER, 100},
                                                                                       {graphic_resource::Type::COMBINED_IMAGE_SAMPLER, 100},
                                                                                       {graphic_resource::Type::SAMPLED_IMAGE, 100},
                                                                                       {graphic_resource::Type::STORAGE_IMAGE, 100},
                                                                                       {graphic_resource::Type::UNIFORM_TEXEL_BUFFER, 100},
                                                                                       {graphic_resource::Type::STORAGE_TEXEL_BUFFER, 100},
                                                                                       {graphic_resource::Type::UNIFORM_BUFFER, 100},
                                                                                       {graphic_resource::Type::STORAGE_BUFFER, 100},
                                                                                   },
                                                                           0));

    data_ = std::make_shared<EngineData>(EngineData{
        .vulkan_instance = instance,
        .physical_device = physical_device,
        .logical_device = device,
        .command_manager = command_manager,
        .graphic_resource_manager = graphic_resource_manager,
        .synchro_manager = synchro_manager});

    VSL_NAMESPACE::Swapchain swapchain(device, surface);
    VSL_NAMESPACE::RenderPass render_pass(swapchain);

    boot_window.emplace(*this, window.data_,
                        std::make_shared<AHO_NAMESPACE::window::WindowData>(AHO_NAMESPACE::window::WindowData{
                            .surface = surface,
                            .swapchain = swapchain,
                            .render_pass = render_pass,
                            .frame_buffer = VSL_NAMESPACE::FrameBuffer(swapchain,
                                                                       render_pass),
                            .image_available = synchro_manager
                                                   .create_semaphore(application_name + "ImageAvailable",
                                                                     command_manager.get_buffer().get_size()),
                            .render_finished = synchro_manager
                                                   .create_semaphore(application_name + "RenderFinished",
                                                                     swapchain.get_swap_image_size()),
                            .in_flight = synchro_manager
                                             .create_fence(application_name + "InFlight",
                                                           command_manager.get_buffer().get_size(), true)}));
}

AHO_NAMESPACE::Window AHO_NAMESPACE::engine::StandardEngine::get_window() {
    return boot_window.value();
}
