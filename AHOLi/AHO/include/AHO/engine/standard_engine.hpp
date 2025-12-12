//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_STANDARDENGINE_H
#define AHO_ALL_STANDARDENGINE_H

#include "graphical_engine.hpp"

namespace AHO_NAMESPACE::engine {
    struct StandardEngine : public GraphicalEngine {
        std::optional<aho::window::Window> boot_window = std::nullopt;

        StandardEngine(std::string applicationName, bool bootOn = true);

        void boot(const std::string& applicationName);

        Window getWindow() override;
    };
}

namespace AHO_NAMESPACE {
    using engine::StandardEngine;
}

#endif //AHO_ALL_STANDARDENGINE_H
