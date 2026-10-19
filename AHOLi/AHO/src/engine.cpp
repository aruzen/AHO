//
// Created by morimoto_hibiki on 2025/10/07.
//

#include <AHO/engine.hpp>
#include <AHO/window.hpp>

#include <memory>

AHO_NAMESPACE::engine::EngineAccessor* AHO_NAMESPACE::engine::EngineAccessor::_instance = nullptr;

AHO_NAMESPACE::engine::EngineAccessor* AHO_NAMESPACE::engine::EngineAccessor::instance() {
    return _instance;
}

AHO_NAMESPACE::engine::EngineAccessor::EngineAccessor() {
    if (_instance == nullptr)
        _instance = this;
}

AHO_NAMESPACE::engine::EngineAccessor::~EngineAccessor() {
    if (_instance == this)
        _instance = nullptr;
}
