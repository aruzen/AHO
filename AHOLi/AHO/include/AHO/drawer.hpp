//
// Created by morimoto_hibiki on 2025/10/22.
//

#ifndef AHO_ALL_DRAWER_H
#define AHO_ALL_DRAWER_H

#include "define.hpp"
#include "window.hpp"

#include "VSL/vulkan/phase.hpp"

#include <tuple>

namespace AHO_NAMESPACE {
    template<class... Ts>
    constexpr auto pack(Ts&&... xs) noexcept;

    /*
     * 引き出しと絵描きをかけます
     */
    template<typename PhaseStream>
    struct DrawStream {
        using ThisType = DrawStream<PhaseStream>;

        PhaseStream phase_stream;

        ThisType& operator<<(AHO_NAMESPACE::concepts::is_std_tuple auto&& args);

        ThisType& operator<<(std::shared_ptr<VSL_NAMESPACE::command::Command> cmd);

        ThisType& operator<<(VSL_NAMESPACE::command::is_command auto&& cmd);

        ThisType& operator<<(std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand> cmd);

        ThisType& operator<<(VSL_NAMESPACE::command::is_delegate_command auto&& cmd);
    };

    template<class... Ts>
    constexpr auto pack(Ts&&... xs) noexcept {
        return std::forward_as_tuple(std::forward<Ts>(xs)...);
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream>& DrawStream<PhaseStream>::operator<<(VSL_NAMESPACE::command::is_command auto&& cmd) {
        phase_stream << std::forward<decltype(cmd)>(cmd);
        return *this;
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream>& DrawStream<PhaseStream>::operator<<(VSL_NAMESPACE::command::is_delegate_command auto&& cmd) {
        phase_stream << std::forward<decltype(cmd)>(cmd);
        return *this;
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream>& DrawStream<PhaseStream>::operator<<(std::shared_ptr<VSL_NAMESPACE::command::Command> cmd) {
        phase_stream << cmd;
        return *this;
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream>& DrawStream<PhaseStream>::operator<<(std::shared_ptr<VSL_NAMESPACE::command::DelegateCommand> cmd) {
        phase_stream << cmd;
        return *this;
    }
} // namespace AHO_NAMESPACE

#endif //AHO_ALL_DRAWER_H
