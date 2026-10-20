#include <AHO/drawer/draw_phase.hpp>
#include <AHO/engine/standard_engine.hpp>
#include <AHO/x/drawer/renderer2d.hpp>
#include <iostream>
#include <optional>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: aho-drawing-demo <compiled-shader-directory>\n";
        return 1;
    }
    aho::StandardEngine engine("AHO 2D drawing");
    aho::x::DrawList list;
    list.rectangle({20, 20, 260, 160}, aho::RGBA(.2f, .3f, .8f, 1));
    list.rectangle({80, 80, 200, 160}, aho::RGBA(1, .3f, .1f, .7f));
    list.triangle({aho::d2::PointF(60, 300), aho::d2::PointF(320, 300), aho::d2::PointF(180, 180)}, aho::RGBA(.2f, 1, .3f, 1));
    std::optional<aho::x::Renderer2D> renderer;
    vsl::RenderPassAccessor previous;
    while (vsl::PureWindow::Update()) {
        const auto window = engine.getWindow();
        const auto size = engine.getWindow().frame_size();
        if (size.value.___AN1 <= 0 || size.value.___AN2 <= 0)
            continue;
        if (!renderer || previous._data != window._data2->render_pass._data) {
            renderer.emplace(engine._data->logical_device, window._data2->render_pass, argv[1]);
            previous = window._data2->render_pass;
        }
        aho::DrawPhase phase(engine);
        if (phase.state == vsl::ComputePhase::State::WaitSubmit)
            (void)renderer->record(phase, list, static_cast<std::uint32_t>(size.value.___AN1), static_cast<std::uint32_t>(size.value.___AN2));
    }
    engine._data->command_manager.waitIdle();
}
