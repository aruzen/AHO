#include <AHO/drawer/draw_phase.hpp>
#include <VSL/vulkan/commands/render_pass_begin.hpp>
#include <VSL/vulkan/commands/render_pass_end.hpp>
#include <utility>
#include <exception>
#include <stdexcept>

thread_local AHO_NAMESPACE::DrawPhase* AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE;

AHO_NAMESPACE::RenderContext AHO_NAMESPACE::DrawPhase::context() const {
    return engine->get_window().render_context();
}

AHO_NAMESPACE::DrawPhase::DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine& engine,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : AHO_NAMESPACE::DrawPhase::DrawPhase(&engine, initialize, finit) {}

AHO_NAMESPACE::DrawPhase::DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine* engine,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : VSL_NAMESPACE::DefaultPhase(engine->data_->command_manager,
                                  engine->get_window().data2->swapchain,
                                  engine->get_window().data2->image_available,
                                  engine->get_window().data2->render_finished,
                                  engine->get_window().data2->in_flight),
      engine(engine),
      initialize(initialize),
      finish(finit) {
    _previous_phase = std::exchange(THREAD_LOCAL_DRAW_PHASE, this);
    try {
        if (state != State::WAIT_SUBMIT)
            return;
        if (initialize) {
            DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
            initialize(stream);
        } else {
            *this << VSL_NAMESPACE::command::RenderPassBegin(engine->get_window().data2->render_pass,
                                                             engine->get_window().data2->frame_buffer);
        }
    } catch (...) {
        restore_previous_phase();
        throw; // 基底クラスのデストラクタがcancel()で取得済み画像を返す。
    }
}

AHO_NAMESPACE::DrawPhase::~DrawPhase() {
    try {
        if (std::uncaught_exceptions() > initial_exceptions) {
            VSL_NAMESPACE::DefaultPhase::cancel();
        } else {
            auto result = VSL_NAMESPACE::DefaultPhase::submit();
            if (result == VSL_NAMESPACE::DefaultPhase::SubmitResult::WANT_RECREATE_SWAPCHAIN)
                engine->get_window().resize(engine->get_window().window_size());
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
    if (state == State::WAIT_SUBMIT) {
        if (_finish_started)
            throw std::logic_error("DrawPhase completion is already in progress");
        _finish_started = true;
        try {
            if (finish) {
                DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
                finish(stream);
            } else {
                *this << VSL_NAMESPACE::command::RenderPassEnd();
            }
        } catch (...) {
            try {
                VSL_NAMESPACE::DefaultPhase::cancel();
            } catch (...) {
            }
            throw;
        }
    }
    return VSL_NAMESPACE::DefaultPhase::submit_commands();
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::Command> cmd) {
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand> cmd) {
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}
