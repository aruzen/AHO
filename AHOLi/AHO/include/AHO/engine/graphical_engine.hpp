//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_GRAPHITICALENGINE_H
#define AHO_ALL_GRAPHITICALENGINE_H

#include "../engine.hpp"
#include "../window.hpp"

namespace AHO_NAMESPACE::engine {
    struct GraphicalEngine : public EngineAccessor {
        GraphicalEngine() = default;
        virtual ~GraphicalEngine() = default;

        GraphicalEngine(const GraphicalEngine&) = default;
        GraphicalEngine& operator=(const GraphicalEngine&) = default;
        GraphicalEngine(GraphicalEngine&&) noexcept = default;
        GraphicalEngine& operator=(GraphicalEngine&&) noexcept = default;

        virtual aho::Window getWindow() = 0;
    };
}


#endif //AHO_ALL_GRAPHITICALENGINE_H
