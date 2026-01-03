//
// Created by morimoto_hibiki on 2025/12/21.
//

#include <AHO/drawer/draw_phase.hpp>
#include <utility>

thread_local aho::DrawPhase* aho::THREAD_LOCAL_DRAW_PHASE;

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
    THREAD_LOCAL_DRAW_PHASE = this;

    if (initialize) {
        DrawStream<vsl::DefaultPhaseStream> stream{vsl::DefaultPhaseStream{this->manager}};
        initialize(stream);
    } else
        *this << vsl::command::RenderPassBegin(engine->getWindow()._data2->render_pass,
                                               engine->getWindow()._data2->frame_buffer);
}

aho::DrawPhase::~DrawPhase() {
    if (finish) {
        DrawStream<vsl::DefaultPhaseStream> stream{vsl::DefaultPhaseStream{this->manager}};
        finish(stream);
    } else
        *this << vsl::command::RenderPassEnd();

    auto result = vsl::DefaultPhase::submit();
    engine->getWindow()._data2->in_flight.wait();
    if (result == vsl::DefaultPhase::SubmitResult::WantRecreateSwapchain)
        engine->getWindow().resize(engine->getWindow().window_size());
    THREAD_LOCAL_DRAW_PHASE = nullptr;
}

aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(std::shared_ptr<vsl::command::__Command> cmd) {
    return {
        .phase_stream = (vsl::DefaultPhaseStream{this->manager} << std::move(cmd))
    };
}

aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(std::shared_ptr<vsl::command::__DelegateCommand> cmd) {
    return {
        .phase_stream = (vsl::DefaultPhaseStream{this->manager} << std::move(cmd))
    };
}
