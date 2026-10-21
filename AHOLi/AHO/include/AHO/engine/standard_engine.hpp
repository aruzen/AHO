//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_STANDARDENGINE_H
#define AHO_ALL_STANDARDENGINE_H

#include "graphical_engine.hpp"

namespace AHO_NAMESPACE::engine {
    struct StandardEngine : public GraphicalEngine {
        std::optional<AHO_NAMESPACE::window::Window> boot_window = std::nullopt;

        StandardEngine(std::string application_name, bool boot_on = true);

        void boot(const std::string& application_name);

        Window get_window() override;
    };
} // namespace AHO_NAMESPACE::engine

namespace AHO_NAMESPACE {
    using engine::StandardEngine;
}

#endif //AHO_ALL_STANDARDENGINE_H
