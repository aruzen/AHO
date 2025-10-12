//
// Created by morimoto_hibiki on 2025/10/10.
//

#include "Window.h"

#include <utility>

aho::window::Window::Window(std::string name, int width, int height, aho::engine::EngineAccessor _engine)
        : VSL_NAMESPACE::Window(std::move(name), width, height) {
    engine = _engine;
    auto surface = this->addPlugin<VSL_NAMESPACE::Surface>(engine._data->vulkan_instance);
    VSL_NAMESPACE::Swapchain swapchain(engine._data->logical_device, surface);
    VSL_NAMESPACE::View<VSL_NAMESPACE::D2> image_view(swapchain);
    VSL_NAMESPACE::RenderPass pass(swapchain);

    _data2 = std::make_shared<AHO_NAMESPACE::window::WindowData>(AHO_NAMESPACE::window::WindowData{
            .surface = surface,
            .swapchain = swapchain,
            .image_view = image_view,
            .render_pass = pass,
            .frame_buffer = VSL_NAMESPACE::FrameBuffer<VSL_NAMESPACE::D2>(swapchain, image_view, pass)
    });
}

aho::window::Window::Window(std::shared_ptr<vsl::Window::WindowData> vsl_data, std::shared_ptr<aho::window::WindowData> aho_data) : VSL_NAMESPACE::Window(std::move(vsl_data)) {
    _data2 = std::move(aho_data);
}

aho::window::Window::~Window() {
    _data2.reset();
    close();
}

bool aho::window::Window::resize(int width, int height) {
    VSL_NAMESPACE::PureWindow::resize(width, height);

    VSL_NAMESPACE::Swapchain swapchain(engine._data->logical_device, _data2->surface);
    VSL_NAMESPACE::View<VSL_NAMESPACE::D2> image_view(swapchain);
}
