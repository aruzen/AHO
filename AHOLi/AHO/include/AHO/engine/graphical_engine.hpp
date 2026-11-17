//
// Created by morimoto_hibiki on 2025/10/21.
//

#ifndef AHO_ALL_GRAPHITICALENGINE_H
#define AHO_ALL_GRAPHITICALENGINE_H

#include "../engine.hpp"
#include "../window.hpp"

namespace AHO_NAMESPACE::engine {
    struct GraphicalEngine {
    private:
        EngineAccessor _runtime;
        friend struct detail::EngineAccess;

    protected:
        void set_runtime(EngineAccessor value) noexcept { _runtime.swap(value); }
        void swap_runtime(EngineAccessor& value) noexcept { _runtime.swap(value); }

    public:
        GraphicalEngine() = default;
        virtual ~GraphicalEngine() = default;

        GraphicalEngine(const GraphicalEngine&) = delete;
        GraphicalEngine& operator=(const GraphicalEngine&) = delete;
        GraphicalEngine(GraphicalEngine&&) = delete;
        GraphicalEngine& operator=(GraphicalEngine&&) = delete;

        EngineAccessor runtime() const { return _runtime; }
        operator EngineAccessor() const { return _runtime; }
        explicit operator bool() const noexcept { return bool(_runtime); }
        virtual AHO_NAMESPACE::Window get_window() = 0;
    };
} // namespace AHO_NAMESPACE::engine

#endif //AHO_ALL_GRAPHITICALENGINE_H
