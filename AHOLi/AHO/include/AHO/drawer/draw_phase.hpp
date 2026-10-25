#ifndef AHO_ALL_DRAW_PHASE_H
#define AHO_ALL_DRAW_PHASE_H

#include <VSL/vsl.hpp>
#include <concepts>
#include "../engine/graphical_engine.hpp"
#include "../drawer.hpp"

namespace AHO_NAMESPACE {
    struct DrawPhase;
    extern thread_local DrawPhase* THREAD_LOCAL_DRAW_PHASE;

    struct DrawPhase : public VSL_NAMESPACE::DefaultPhase {
        explicit DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine& engine,
                           std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize = nullptr,
                           std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit = nullptr);

        explicit DrawPhase(AHO_NAMESPACE::engine::GraphicalEngine* engine,
                           std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize = nullptr,
                           std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> finit = nullptr);

        std::function<void(DrawStream<VSL_NAMESPACE::DefaultPhaseStream>&)> initialize, finish;
        AHO_NAMESPACE::engine::GraphicalEngine* engine;

        ~DrawPhase();

        SubmitResult submit_commands() override;

        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> operator<<(AHO_NAMESPACE::concepts::is_std_tuple auto&&);

        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> operator<<(std::shared_ptr<VSL_NAMESPACE::command::Command> cmd);

        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> operator<<(VSL_NAMESPACE::command::is_command auto cmd);

        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> operator<<(std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand> cmd);

        DrawStream<VSL_NAMESPACE::DefaultPhaseStream> operator<<(VSL_NAMESPACE::command::is_delegate_command auto cmd);

    private:
        bool _finish_started = false;
        DrawPhase* _previous_phase = nullptr;
        void restore_previous_phase() noexcept;
        static DrawPhase* CurrentForManager(const VSL_NAMESPACE::CommandManager& manager) noexcept;
        template<typename>
        friend struct DrawStream;
        friend struct AHO_NAMESPACE::window::Window;
    };

    AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(AHO_NAMESPACE::concepts::is_std_tuple auto&& t) {
        AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> phase{
            .phase_stream = VSL_NAMESPACE::DefaultPhaseStream{this->manager}};
        phase << std::forward<decltype(t)>(t);
        return phase;
    }

    AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(VSL_NAMESPACE::command::is_command auto cmd) {
        return {
            .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
    }

    AHO_NAMESPACE::DrawStream<VSL_NAMESPACE::DefaultPhaseStream> AHO_NAMESPACE::DrawPhase::operator<<(VSL_NAMESPACE::command::is_delegate_command auto cmd) {
        return {
            .phase_stream = (VSL_NAMESPACE::DefaultPhaseStream{this->manager} << std::move(cmd))};
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream>& DrawStream<PhaseStream>::operator<<(AHO_NAMESPACE::concepts::is_std_tuple auto&& args) {
        auto* phase = THREAD_LOCAL_DRAW_PHASE;
        if constexpr (requires { { phase_stream.manager } -> std::convertible_to<VSL_NAMESPACE::CommandManager>; })
            phase = DrawPhase::CurrentForManager(phase_stream.manager);
        std::apply(
            [](auto&&... xs) { draw(std::forward<decltype(xs)>(xs)...); },
            std::tuple_cat(std::forward<decltype(args)>(args), std::tie(phase)));
        return *this;
    }
} // namespace AHO_NAMESPACE

#endif
