#include <AHO/drawer/draw_phase.hpp>
#include <utility>
#include <exception>
#include <stdexcept>

thread_local aho::DrawPhase *aho::THREAD_LOCAL_DRAW_PHASE;

aho::DrawPhase::DrawPhase(aho::engine::GraphicalEngine &engine,
                          std::function<void(DrawStream<vsl::DefaultPhaseStream> &)> initialize,
                          std::function<void(DrawStream<vsl::DefaultPhaseStream> &)> finit)
    : aho::DrawPhase::DrawPhase(&engine, initialize, finit) {}

aho::DrawPhase::DrawPhase(aho::engine::GraphicalEngine *engine,
                          std::function<void(DrawStream<vsl::DefaultPhaseStream> &)> initialize,
                          std::function<void(DrawStream<vsl::DefaultPhaseStream> &)> finit)
    : VSL_NAMESPACE::DefaultPhase(engine->_data->command_manager,
                                  engine->getWindow()._data2->swapchain,
                                  engine->getWindow()._data2->image_available,
                                  engine->getWindow()._data2->render_finished,
                                  engine->getWindow()._data2->in_flight),
      engine(engine),
      initialize(initialize),
      finish(finit) {
    auto *previous = THREAD_LOCAL_DRAW_PHASE;
    THREAD_LOCAL_DRAW_PHASE = this;
    try {
        if (state != State::WaitSubmit)
            return;
        if (initialize) {
            DrawStream<vsl::DefaultPhaseStream> stream{vsl::DefaultPhaseStream{this->manager}};
            initialize(stream);
        } else {
            *this << vsl::command::RenderPassBegin(engine->getWindow()._data2->render_pass,
                                                   engine->getWindow()._data2->frame_buffer);
        }
    } catch (...) {
        THREAD_LOCAL_DRAW_PHASE = previous;
        throw; // 基底クラスのデストラクタがcancel()で取得済み画像を返す。
    }
}

aho::DrawPhase::~DrawPhase() {
    try {
        if (std::uncaught_exceptions() > initialExceptions) {
            vsl::DefaultPhase::cancel();
        } else {
            auto result = vsl::DefaultPhase::submit();
            if (result == vsl::DefaultPhase::SubmitResult::WantRecreateSwapchain)
                engine->getWindow().resize(engine->getWindow().window_size());
        }
    } catch (...) {
        try {
            vsl::DefaultPhase::cancel();
        } catch (...) {
        }
        try {
            vsl::loggingln("DrawPhase completion failed; recording cancelled");
        } catch (...) {
        }
    }
    THREAD_LOCAL_DRAW_PHASE = nullptr;
}

aho::DrawPhase::SubmitResult aho::DrawPhase::submitCommands() {
    if (state == State::WaitSubmit) {
        if (finishStarted)
            throw std::logic_error("DrawPhase completion is already in progress");
        finishStarted = true;
        try {
            if (finish) {
                DrawStream<vsl::DefaultPhaseStream> stream{vsl::DefaultPhaseStream{this->manager}};
                finish(stream);
            } else {
                *this << vsl::command::RenderPassEnd();
            }
        } catch (...) {
            try {
                vsl::DefaultPhase::cancel();
            } catch (...) {
            }
            throw;
        }
    }
    return vsl::DefaultPhase::submitCommands();
}

aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(std::shared_ptr<vsl::command::__Command> cmd) {
    return {
        .phase_stream = (vsl::DefaultPhaseStream{this->manager} << std::move(cmd))};
}

aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(std::shared_ptr<vsl::command::__DelegateCommand> cmd) {
    return {
        .phase_stream = (vsl::DefaultPhaseStream{this->manager} << std::move(cmd))};
}
