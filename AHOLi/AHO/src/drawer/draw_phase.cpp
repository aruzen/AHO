//
// Created by morimoto_hibiki on 2025/12/21.
//

#include <AHO/drawer/draw_phase.hpp>

aho::DrawPhase::DrawPhase(aho::engine::GraphicalEngine &engine)
        : aho::DrawPhase::DrawPhase(&engine) {}

aho::DrawPhase::DrawPhase(aho::engine::GraphicalEngine *engine)
        : vsl::DefaultPhase(engine->_data->command_manager,
                            engine->getWindow()._data2->swapchain,
                            engine->getWindow()._data2->image_available,
                            engine->getWindow()._data2->render_finished,
                            engine->getWindow()._data2->in_flight),
          engine(engine) {}

aho::DrawPhase::~DrawPhase() {
    auto result = vsl::DefaultPhase::submit();
    engine->getWindow()._data2->in_flight.wait();
    if (result == vsl::DefaultPhase::SubmitResult::WantRecreateSwapchain)
        engine->getWindow().resize(engine->getWindow().window_size());
}
