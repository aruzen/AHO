#include <AHO/drawer/draw_phase.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <VSL/lifetime/phase_liveness.hpp>
#include <utility>
#include <exception>
#include <stdexcept>

struct AHO_NAMESPACE::DrawPhase::Init {
    AHO_NAMESPACE::engine::GraphicalEngine* engine;
    AHO_NAMESPACE::Window window;
    VSL_NAMESPACE::CommandManager manager;
};

thread_local AHO_NAMESPACE::DrawPhase* AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE;

AHO_NAMESPACE::RenderContext AHO_NAMESPACE::DrawPhase::context() const {
    return _window.render_context();
}

AHO_NAMESPACE::DrawPhase::DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine& engine,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : AHO_NAMESPACE::DrawPhase::DrawPhase(&engine, initialize, finit) {}

AHO_NAMESPACE::DrawPhase::DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine* engine,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : DrawPhase(Prepare(engine), std::move(initialize), std::move(finit)) {}

AHO_NAMESPACE::DrawPhase::Init AHO_NAMESPACE::DrawPhase::Prepare(AHO_NAMESPACE::engine::GraphicalEngine* engine) {
    if (!engine || !engine->data())
        throw std::invalid_argument("DrawPhase requires a booted engine");
    const auto runtime = engine->data();
    const auto manager = runtime->command_manager;
    const auto buffer = runtime->command_manager.get_buffer().data_.get();
    auto window = engine->get_window();
    if (!window || !window.data2)
        throw std::logic_error("DrawPhase requires a live drawing window");
    const auto target = window.data2;
    const auto size = window.frame_size();
    if (size.value.x.value == 0 || size.value.y.value == 0)
        target->deferred_usage = target->deferred_usage.value_or(VSL_NAMESPACE::ImageType::NONE);
    if (size.value.x.value > 0 && size.value.y.value > 0 &&
        (target->pending_generation || target->deferred_usage))
        window.recreate_swapchain();
    if (!window || window.data2 != target || runtime->command_manager.data_ != manager.data_ ||
        runtime->command_manager.get_buffer().data_.get() != buffer)
        throw std::logic_error("Window closed while preparing a DrawPhase");
    return {engine, std::move(window), manager};
}

bool AHO_NAMESPACE::DrawPhase::acquisition_ready() {
    VSL_NAMESPACE::lifetime::PhaseCall call(*this);
    if (!_window || !_window.data2 || _window.data2->pending_generation || _window.data2->deferred_usage)
        return false;
    const auto size = _window.frame_size();
    if (!call.alive())
        return false;
    if (size.value.x.value == 0 || size.value.y.value == 0)
        _window.data2->deferred_usage = _window.data2->deferred_usage.value_or(VSL_NAMESPACE::ImageType::NONE);
    return _window && size.value.x.value > 0 && size.value.y.value > 0;
}

AHO_NAMESPACE::DrawPhase::DrawPhase(Init init,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : VSL_NAMESPACE::DefaultPhase(init.manager,
                                  init.window.data2->swapchain,
                                  init.window.data2->image_available,
                                  init.window.data2->render_finished,
                                  init.window.data2->in_flight,
                                  true,
                                  init.window.data2->acquire_fences.data_
                                      ? std::optional<VSL_NAMESPACE::FenceHolder>{init.window.data2->acquire_fences}
                                      : std::nullopt),
      engine(init.engine),
      initialize(initialize),
      finish(finit),
      _window(std::move(init.window)) {
    VSL_NAMESPACE::lifetime::PhaseCall call(*this);
    _previous_phase = std::exchange(THREAD_LOCAL_DRAW_PHASE, this);
    try {
        setup();
    } catch (...) {
        if (call.alive())
            restore_previous_phase();
        throw; // 基底クラスのデストラクタがcancel()で取得済み画像を返す。
    }
}

void AHO_NAMESPACE::DrawPhase::setup() {
    VSL_NAMESPACE::lifetime::PhaseCall call(*this);
    if (state != State::BEFORE_SETUP)
        return;
    VSL_NAMESPACE::DefaultPhase::setup();
    if (!call.alive() || !recording())
        return;
    const auto callback = initialize;
    if (callback) {
        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
        callback(stream);
    } else {
        *this << VSL_NAMESPACE::command::RenderPassBegin(_window.data2->render_pass, _window.data2->frame_buffer);
    }
}

AHO_NAMESPACE::DrawPhase::~DrawPhase() {
    VSL_NAMESPACE::lifetime::PhaseCall::Invalidate(*this);
    try {
        if (std::uncaught_exceptions() > initial_exceptions) {
            VSL_NAMESPACE::DefaultPhase::cancel();
        } else {
            auto result = VSL_NAMESPACE::DefaultPhase::submit();
            if (result == VSL_NAMESPACE::DefaultPhase::SubmitResult::WANT_RECREATE_SWAPCHAIN && _window && _window.data2)
                _window.recreate_swapchain();
        }
    } catch (...) {
        try {
            VSL_NAMESPACE::DefaultPhase::cancel();
        } catch (...) {
        }
        try {
            VSL_NAMESPACE::loggingln("DrawPhase completion failed; recording cancelled");
        } catch (...) {
        }
    }
    restore_previous_phase();
}

void AHO_NAMESPACE::DrawPhase::restore_previous_phase() noexcept {
    auto** current = &THREAD_LOCAL_DRAW_PHASE;
    // 外側が先に破棄された場合も、内側から破棄済みのPhaseへ戻らないようにつなぎ直す。
    while (*current && *current != this)
        current = &(*current)->_previous_phase;
    if (*current == this)
        *current = _previous_phase;
}

AHO_NAMESPACE::DrawPhase* AHO_NAMESPACE::DrawPhase::CurrentForManager(const VSL_NAMESPACE::CommandManager& manager) noexcept {
    auto* phase = THREAD_LOCAL_DRAW_PHASE;
    while (phase && phase->manager.data_ != manager.data_)
        phase = phase->_previous_phase;
    return phase;
}

AHO_NAMESPACE::DrawPhase::SubmitResult AHO_NAMESPACE::DrawPhase::submit_commands() {
    VSL_NAMESPACE::lifetime::PhaseCall call(*this);
    if (state == State::WAIT_SUBMIT) {
        if (_finish_started)
            throw std::logic_error("DrawPhase completion is already in progress");
        _finish_started = true;
        try {
            const auto callback = finish;
            if (callback) {
                DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
                callback(stream);
            } else {
                *this << VSL_NAMESPACE::command::RenderPassEnd();
            }
        } catch (...) {
            try {
                if (call.alive())
                    VSL_NAMESPACE::DefaultPhase::cancel();
            } catch (...) {
            }
            throw;
        }
    }
    if (!call.alive())
        return SubmitResult::FAILED;
    return VSL_NAMESPACE::DefaultPhase::submit_commands();
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::Command> cmd) {
    require_recording();
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand> cmd) {
    require_recording();
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}
