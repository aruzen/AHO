//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_STANDARDENGINE_H
#define AHO_ALL_STANDARDENGINE_H

#include "../Engine.hpp"
#include "../Window.h"

namespace AHO_NAMESPACE::engine {
    struct StandardEngine : public EngineAccessor {
        std::optional<aho::window::Window> boot_window = std::nullopt;

        StandardEngine(std::string applicationName, bool bootOn = true);

        void boot(std::string applicationName);

        std::optional<aho::window::Window> supportWindow() override;
    };
}

namespace AHO_NAMESPACE {
    using engine::StandardEngine;
}

#endif //AHO_ALL_STANDARDENGINE_H
