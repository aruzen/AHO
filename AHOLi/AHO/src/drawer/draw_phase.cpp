//
// Created by morimoto_hibiki on 2025/12/21.
//

#include <AHO/drawer/draw_phase.hpp>
#include <utility>

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
    THREAD_LOCAL_DRAW_PHASE = this;

    if (initialize) {
        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
        initialize(stream);
    } else
        *this << VSL_NAMESPACE::command::RenderPassBegin(engine->getWindow()._data2->render_pass,
                                                         engine->getWindow()._data2->frame_buffer);
}

AHO_NAMESPACE::DrawPhase::~DrawPhase() {
    if (finish) {
        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> stream{VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
        finish(stream);
    } else
        *this << VSL_NAMESPACE::command::RenderPassEnd();

    auto result = VSL_NAMESPACE::DefaultPhase::submit();
    engine->getWindow()._data2->in_flight.wait();
    if (result == VSL_NAMESPACE::DefaultPhase::SubmitResult::WantRecreateSwapchain)
        engine->getWindow().resize(engine->getWindow().window_size());
    THREAD_LOCAL_DRAW_PHASE = nullptr;
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::__Command> cmd) {
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}

AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(std::shared_ptr<VSL_NAMESPACE::command::__DelegateCommand> cmd) {
    return {
        .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
}
