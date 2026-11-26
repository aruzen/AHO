#pragma once
#include "engine_state.hpp"
#include <AHO/engine/graphical_engine.hpp>
#include <stdexcept>
#include <utility>

namespace AHO_NAMESPACE::engine::detail {
    struct EngineAccess {
        static const std::shared_ptr<EngineData>& State(const EngineAccessor& value) noexcept { return value._data; }
        static std::shared_ptr<EngineData>& State(EngineAccessor& value) noexcept { return value._data; }
        static std::shared_ptr<EngineData> State(EngineAccessor&& value) noexcept { return std::move(value._data); }
        static std::shared_ptr<EngineData> State(const EngineAccessor&& value) noexcept { return value._data; }
        static const std::shared_ptr<EngineData>& State(const GraphicalEngine& value) noexcept { return value._runtime._data; }
        static std::shared_ptr<EngineData>& State(GraphicalEngine& value) noexcept { return value._runtime._data; }
        static std::shared_ptr<EngineData> State(GraphicalEngine&& value) noexcept { return std::move(value._runtime._data); }
        static std::shared_ptr<EngineData> State(const GraphicalEngine&& value) noexcept { return value._runtime._data; }
        static EngineAccessor FromState(std::shared_ptr<EngineData> value) { return EngineAccessor(std::move(value)); }

        static const EngineData& Require(const EngineAccessor& value) {
            if (!value._data)
                throw std::logic_error("Engine requires initialized services");
            return *value._data;
        }
    };
} // namespace AHO_NAMESPACE::engine::detail
