//
// Created by morimoto_hibiki on 2025/10/07.
//

#include "Engine.hpp"
#include "Window.h"

#include <memory>

aho::engine::EngineAccessor* aho::engine::EngineAccessor::_instance = nullptr;

aho::engine::EngineAccessor *aho::engine::EngineAccessor::instance() {
    return _instance;
}

aho::engine::EngineAccessor::EngineAccessor() {
    if (_instance == nullptr)
        _instance = this;
}

std::optional<AHO_NAMESPACE::window::Window> aho::engine::EngineAccessor::supportWindow() {
    return std::nullopt;
}