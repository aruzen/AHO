//
// Created by morimoto_hibiki on 2025/10/22.
//

#include <AHO/drawer.hpp>

template<typename PhaseStream>
aho::DrawStream<PhaseStream> &aho::DrawStream<PhaseStream>::operator<<(std::shared_ptr<vsl::command::__Command> cmd) {
    phase_stream << cmd;
    return *this;
}

template<typename PhaseStream>
aho::DrawStream<PhaseStream> &aho::DrawStream<PhaseStream>::operator<<(std::shared_ptr<vsl::command::__DelegateCommand> cmd) {
    phase_stream << cmd;
    return *this;
}