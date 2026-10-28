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

        GraphicalEngine(const GraphicalEngine&) = delete;
        GraphicalEngine& operator=(const GraphicalEngine&) = delete;
        GraphicalEngine(GraphicalEngine&&) = delete;
        GraphicalEngine& operator=(GraphicalEngine&&) = delete;

        virtual AHO_NAMESPACE::Window get_window() = 0;
    };
} // namespace AHO_NAMESPACE::engine

#endif //AHO_ALL_GRAPHITICALENGINE_H
