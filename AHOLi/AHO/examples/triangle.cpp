#include <AHO/drawer/standard_drawer.hpp>
#include <AHO/drawer/draw_phase.hpp>
#include <AHO/engine/standard_engine.hpp>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto limit = argc > 1 ? std::stoull(argv[1]) : 0;
        aho::StandardEngine engine("AHO Typed Triangle");
        auto window = engine.get_window();
        auto commands = window.render_context().commands();
        window.set_title("AHO Typed Triangle");
        const aho::Triangle<float, vsl::D2> triangle({-.7f, -.6f}, {.7f, -.6f}, {0, .7f});
        std::size_t frames = 0;
        while ((!limit || frames < limit) && vsl::PureWindow::Update()) {
            aho::DrawPhase phase(engine);
            phase << aho::pack(triangle, aho::RGBA(.2f, .7f, 1, 1));
            ++frames;
        }
        commands.wait_idle();
        window.close();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
