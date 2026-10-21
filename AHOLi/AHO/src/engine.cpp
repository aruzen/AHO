//
// Created by morimoto_hibiki on 2025/10/07.
//

#include <AHO/engine.hpp>
#include <AHO/window.hpp>

#include <memory>

AHO_NAMESPACE::engine::EngineAccessor* AHO_NAMESPACE::engine::EngineAccessor::INSTANCE = nullptr;

AHO_NAMESPACE::engine::EngineAccessor* AHO_NAMESPACE::engine::EngineAccessor::Instance() {
    return INSTANCE;
}

AHO_NAMESPACE::engine::EngineAccessor::EngineAccessor() {
    if (INSTANCE == nullptr)
        INSTANCE = this;
}

AHO_NAMESPACE::engine::EngineAccessor::~EngineAccessor() {
    if (INSTANCE == this)
        INSTANCE = nullptr;
}
