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
    constexpr auto pack(Ts&&... xs) noexcept {
        return std::forward_as_tuple(std::forward<Ts>(xs)...);
    }

    /*
     * 引き出しと絵描きをかけます
     */
    template<typename PhaseStream>
    struct DrawStream {
        using this_type = DrawStream<PhaseStream>;

        PhaseStream phase_stream;

        this_type &operator<<(aho::concepts::is_std_tuple auto&& args);

        this_type &operator<<(std::shared_ptr<vsl::command::__Command> cmd);

        this_type &operator<<(vsl::command::is_command auto&& cmd);

        this_type &operator<<(std::shared_ptr<vsl::command::__DelegateCommand> cmd);

        this_type &operator<<(vsl::command::is_delegate_command auto&& cmd);
    };

    template<typename PhaseStream>
    DrawStream<PhaseStream> &DrawStream<PhaseStream>::operator<<(vsl::command::is_command auto&& cmd) {
        phase_stream << std::forward<decltype(cmd)>(cmd);
        return *this;
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream> &DrawStream<PhaseStream>::operator<<(vsl::command::is_delegate_command auto&& cmd) {
        phase_stream << std::forward<decltype(cmd)>(cmd);
        return *this;
    }
}

#endif //AHO_ALL_DRAWER_H
