//
// Created by morimoto_hibiki on 2025/12/21.
//

#ifndef AHO_ALL_DRAW_PHASE_H
#define AHO_ALL_DRAW_PHASE_H

#include <VSL/vsl.hpp>
#include "../engine/graphical_engine.hpp"
#include "../drawer.hpp"

namespace AHO_NAMESPACE {
    struct DrawPhase;
    extern thread_local DrawPhase* THREAD_LOCAL_DRAW_PHASE;

    struct DrawPhase : public VSL_NAMESPACE::DefaultPhase {
        explicit DrawPhase(aho::engine::GraphicalEngine &engine);

        explicit DrawPhase(aho::engine::GraphicalEngine *engine);

        aho::engine::GraphicalEngine * engine;

        ~DrawPhase();

        template<typename ...Args>
        DrawStream<vsl::DefaultPhaseStream> operator<<(std::tuple<Args...> &&);

        DrawStream<vsl::DefaultPhaseStream> operator<<(std::shared_ptr<vsl::command::__Command> cmd);

        DrawStream<vsl::DefaultPhaseStream> operator<<(vsl::command::is_command auto cmd);

        DrawStream<vsl::DefaultPhaseStream> operator<<(std::shared_ptr<vsl::command::__DelegateCommand> cmd);

        DrawStream<vsl::DefaultPhaseStream> operator<<(vsl::command::is_delegate_command auto cmd);
    };

    template<typename... Args>
    aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(std::tuple<Args...> &&t) {
        aho::DrawStream<vsl::DefaultPhaseStream> phase{
                .phase_stream = vsl::DefaultPhaseStream{this->manager}
        };
        phase << std::move(t);
        return phase;
    }

    aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(vsl::command::is_command auto cmd) {
        return {
                .phase_stream = (vsl::DefaultPhaseStream{this->manager} << std::move(cmd))
        };
    }

    aho::DrawStream<vsl::DefaultPhaseStream> aho::DrawPhase::operator<<(vsl::command::is_delegate_command auto cmd) {
        return {
                .phase_stream = (vsl::DefaultPhaseStream{this->manager} << std::move(cmd))
        };
    }

    template<typename PhaseStream>
    template<typename... Args>
    DrawStream<PhaseStream> &DrawStream<PhaseStream>::operator<<(std::tuple<Args...>&& args) {
        std::apply(
            [](auto&&... xs){ draw(std::forward<decltype(xs)>(xs)...); },
            std::tuple_cat(std::forward<decltype(args)>(args), std::tie(THREAD_LOCAL_DRAW_PHASE))
        );
        return *this;
    }
}

#endif //AHO_ALL_DRAW_PHASE_H
