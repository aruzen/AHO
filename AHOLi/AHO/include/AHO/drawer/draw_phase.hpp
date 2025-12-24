//
// Created by morimoto_hibiki on 2025/12/21.
//

#ifndef AHO_ALL_DRAW_PHASE_H
#define AHO_ALL_DRAW_PHASE_H

#include <VSL/vsl.hpp>
#include "../engine/graphical_engine.hpp"

namespace AHO_NAMESPACE {
    struct DrawPhase : public VSL_NAMESPACE::DefaultPhase {
        DrawPhase(aho::engine::GraphicalEngine& engine);

        DrawPhase(aho::engine::GraphicalEngine* engine);

        aho::engine::GraphicalEngine * engine;

        ~DrawPhase();
    };
}

#endif //AHO_ALL_DRAW_PHASE_H
