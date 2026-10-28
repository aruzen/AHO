//
// Created by morimoto_hibiki on 2025/10/10.
//

#ifndef AHO_ALL_STANDARDENGINE_H
#define AHO_ALL_STANDARDENGINE_H

#include "graphical_engine.hpp"

namespace AHO_NAMESPACE::engine {
    struct InitialWindowConfig {
        // 空ならapplication_nameをタイトルに使う。
        std::string title;
        int width = 400;
        int height = 400;
    };

    struct StandardEngineConfig {
        EngineConfig engine;
        InitialWindowConfig initial_window;
        bool register_as_default = true;
    };

    struct StandardEngine : public GraphicalEngine {
        std::optional<AHO_NAMESPACE::window::Window> boot_window = std::nullopt;

        StandardEngine(std::string application_name, bool boot_on = true);
        explicit StandardEngine(StandardEngineConfig config, bool boot_on = true);

        void boot();
        void boot(const std::string& application_name);
        const StandardEngineConfig& config() const;

        Window get_window() override;

    private:
        StandardEngineConfig _config;
        std::optional<DefaultEngineRegistration> _default_registration;
    };
} // namespace AHO_NAMESPACE::engine

namespace AHO_NAMESPACE {
    using engine::StandardEngine;
}

#endif //AHO_ALL_STANDARDENGINE_H
