#include <AHO/drawer/draw_phase.hpp>
#include <utility>
#include <exception>
#include <stdexcept>

thread_local AHO_NAMESPACE::DrawPhase* AHO_NAMESPACE::THREAD_LOCAL_DRAW_PHASE;

AHO_NAMESPACE::DrawPhase::DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine& engine,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : AHO_NAMESPACE::DrawPhase::DrawPhase(&engine, initialize, finit) {}

AHO_NAMESPACE::DrawPhase::DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine* engine,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize,
                                    std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit)
    : VSL_NAMESPACE::DefaultPhase(engine->_data->command_manager,
                                  engine->getWindow()._data2->swapchain,
                                  engine->getWindow()._data2->image_available,
                                  engine->getWindow()._data2->render_finished,
                                  engine->getWindow()._data2->in_flight),
      engine(engine),
      initialize(initialize),
      finish(finit) {
    auto* previous = THREAD_LOCAL_DRAW_PHASE;
    THREAD_LOCAL_DRAW_PHASE = this;
    try {
        if (state != State::WaitSubmit)
            return;
        if (initialize) {
            DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
            initialize(stream);
        } else {
            *this << VSL_NAMESPACE::command::RenderPassBegin(engine->getWindow()._data2->render_pass,
                                                             engine->getWindow()._data2->frame_buffer);
        }
    } catch (...) {
        THREAD_LOCAL_DRAW_PHASE = previous;
        throw; // 基底クラスのデストラクタがcancel()で取得済み画像を返す。
    }
}

AHO_NAMESPACE::DrawPhase::~DrawPhase() {
    try {
        if (std::uncaught_exceptions() > initialExceptions) {
            VSL_NAMESPACE::DefaultPhase::cancel();
        } else {
            auto result = VSL_NAMESPACE::DefaultPhase::submit();
            if (result == VSL_NAMESPACE::DefaultPhase::SubmitResult::WantRecreateSwapchain)
                engine->getWindow().resize(engine->getWindow().window_size());
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
    THREAD_LOCAL_DRAW_PHASE = nullptr;
}

AHO_NAMESPACE::DrawPhase::SubmitResult AHO_NAMESPACE::DrawPhase::submitCommands() {
    if (state == State::WaitSubmit) {
        if (finishStarted)
            throw std::logic_error("DrawPhase completion is already in progress");
        finishStarted = true;
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
    return VSL_NAMESPACE::DefaultPhase::submitCommands();
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::__Command> cmd) {
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::__DelegateCommand> cmd) {
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}
