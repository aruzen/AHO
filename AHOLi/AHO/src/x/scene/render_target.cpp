#include <AHO/x/scene/render_target.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <algorithm>
#include <optional>
#include <stdexcept>

namespace AHO_NAMESPACE::x::scene {
    namespace {
        Window& requireWindow(VSL_NAMESPACE::PureWindow* native) {
            auto* window = dynamic_cast<Window*>(native);
            if (!window || !*window || !window->_data2)
                throw std::invalid_argument("RenderTarget requires a live AHO window");
            return *window;
        }
    }

    struct RenderTarget::Listener {
        std::function<void()> callback;
    };

    struct RenderTarget::Data : window::WindowResizeHookPlugin {
        using Depth = VSL_NAMESPACE::Image<VSL_NAMESPACE::ImageType::DepthStencilAttachment>;
        engine::EngineAccessor engine;
        std::weak_ptr<VSL_NAMESPACE::PureWindow::WindowData> native;
        std::weak_ptr<window::WindowData> drawing;
        VSL_NAMESPACE::SwapchainAccessor swapchain;
        std::optional<VSL_NAMESPACE::RenderPass> pass;
        std::vector<Depth> depths;
        std::vector<VSL_NAMESPACE::FrameBuffer> frames;
        std::vector<std::weak_ptr<Listener>> listeners;
        std::array<unsigned, 2> size{};
        bool ready = false;

        explicit Data(VSL_NAMESPACE::PureWindow* window)
            : WindowResizeHookPlugin(window, [this](Window* current) { rebuild(*current); }),
              engine(requireWindow(window).engine), native(window->_data), drawing(requireWindow(window)._data2) {
            rebuild(requireWindow(window));
        }

        Window liveWindow() const {
            auto n = native.lock();
            auto d = drawing.lock();
            if (!n || !n->window_handle || !d)
                throw std::logic_error("RenderTarget's window has closed");
            return Window(engine, std::move(n), std::move(d));
        }

        void checkReady() const {
            (void)liveWindow();
            if (!ready)
                throw std::logic_error("RenderTarget reconstruction did not complete");
        }

        void rebuild(Window& window) {
            // Initial construction has no users yet. Subsequent calls come from Window after GPU idle.
            ready = false;
            auto next = window._data2->swapchain;
            const VSL_NAMESPACE::Viewport viewport(next);
            const std::array nextSize{unsigned(viewport.width), unsigned(viewport.height)};
            auto nextPass = pass;
            if (!nextPass || nextPass->description().attachments.front().format.type() != next.getFormat().type()) {
                VSL_NAMESPACE::RenderPassDescription description;
                description.addColor({.format = next.getFormat(), .finalLayout = VSL_NAMESPACE::ImageLayout::Present});
                description.setDepthStencil({.format = VSL_NAMESPACE::data_format::Depth32Float,
                                             .store = VSL_NAMESPACE::StoreOp::DontCare,
                                             .finalLayout = VSL_NAMESPACE::ImageLayout::DepthStencilAttachmentOptimal});
                nextPass.emplace(engine._data->logical_device, std::move(description));
            }
            std::vector<Depth> nextDepths;
            std::vector<VSL_NAMESPACE::FrameBuffer> nextFrames;
            for (auto image : next.images) {
                nextDepths.emplace_back(engine._data->logical_device, nextSize[0], nextSize[1], VSL_NAMESPACE::data_format::Depth32Float);
                nextFrames.emplace_back(*nextPass, nextSize[0], nextSize[1],
                                        std::vector<VSL_NAMESPACE::ImageAccessor>{image, nextDepths.back()});
            }
            frames = std::move(nextFrames);
            depths = std::move(nextDepths);
            pass = std::move(nextPass);
            swapchain = std::move(next);
            size = nextSize;
            ready = true;

            std::erase_if(listeners, [](const auto& listener) { return listener.expired(); });
            const auto callbacks = listeners;
            for (const auto& candidate : callbacks) {
                if (!window || !window._data2)
                    break;
                if (auto listener = candidate.lock())
                    listener->callback();
            }
        }
    };

    RenderTarget::RenderTarget(engine::GraphicalEngine& engine) {
        if (THREAD_LOCAL_DRAW_PHASE)
            throw std::logic_error("Create RenderTarget outside a DrawPhase");
        auto window = engine.getWindow();
        data = window.add_plugin<Data>();
    }

    RenderTarget::~RenderTarget() {
        if (data) {
            if (auto native = data->native.lock())
                VSL_NAMESPACE::PureWindow(native).remove_plugin(data);
            data->engine._data->command_manager.waitIdle();
        }
    }

    Window RenderTarget::window() const { return data->liveWindow(); }
    std::array<unsigned, 2> RenderTarget::extent() const { data->checkReady(); return data->size; }
    VSL_NAMESPACE::RenderPassAccessor RenderTarget::renderPass() const { data->checkReady(); return *data->pass; }
    VSL_NAMESPACE::SwapchainAccessor RenderTarget::swapchain() const { data->checkReady(); return data->swapchain; }

    RenderTarget::Subscription RenderTarget::onRecreated(std::function<void()> callback) {
        if (!callback)
            throw std::invalid_argument("RenderTarget callback is empty");
        Subscription subscription;
        subscription.listener = std::make_shared<Listener>(Listener{std::move(callback)});
        data->listeners.emplace_back(subscription.listener);
        return subscription;
    }

    void RenderTarget::begin(DrawPhase& phase) {
        data->checkReady();
        if (phase.engine->_data != data->engine._data || phase.swapchain._data != data->swapchain._data ||
            phase.state != VSL_NAMESPACE::ComputePhase::State::WaitSubmit)
            throw std::invalid_argument("RenderTarget requires a recording DrawPhase on its window");
        phase << VSL_NAMESPACE::command::RenderPassBegin(
            data->frames.at(phase.getImageIndex()),
            {VSL_NAMESPACE::ClearColor{.1f, .2f, .23f, 1}, VSL_NAMESPACE::ClearDepthStencil{1, 0}});
    }
}
