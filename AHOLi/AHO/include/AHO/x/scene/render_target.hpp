#pragma once
#include <AHO/window.hpp>
#include <array>
#include <functional>
#include <memory>

namespace AHO_NAMESPACE {
    struct DrawPhase;
    namespace engine {
        struct GraphicalEngine;
    }
}

namespace AHO_NAMESPACE::x::scene {
    // A window plugin owns the color+depth target. The handle unregisters it on destruction.
    // Use on the window/render thread; construct outside a DrawPhase.
    // RenderTarget must outlive the renderers using it.
    class RenderTarget {
        struct Data;
        struct Listener;
        std::shared_ptr<Data> data;

    public:
        // Keeping this token alive keeps the callback registered. Dropping it disconnects it.
        class Subscription {
            friend class RenderTarget;
            std::shared_ptr<Listener> listener;

        public:
            Subscription() = default;
            Subscription(Subscription&&) noexcept = default;
            Subscription& operator=(Subscription&&) noexcept = default;
            Subscription(const Subscription&) = delete;
            Subscription& operator=(const Subscription&) = delete;
        };

        explicit RenderTarget(engine::GraphicalEngine& engine);
        ~RenderTarget();
        RenderTarget(const RenderTarget&) = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;

        Window window() const;
        std::array<unsigned, 2> extent() const;
        VSL_NAMESPACE::RenderPassAccessor renderPass() const;
        VSL_NAMESPACE::SwapchainAccessor swapchain() const;
        // Called after the target is rebuilt, while GPU idle. No initial call.
        // Listeners added during notification start on the next recreation.
        // Callbacks must not submit GPU work or recreate the swapchain.
        Subscription onRecreated(std::function<void()> callback);
        // Starts the shared color+depth pass. DrawPhase ends it and submits as usual.
        void begin(DrawPhase& phase);
    };
}
