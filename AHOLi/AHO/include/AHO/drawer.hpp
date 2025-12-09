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
    /*
     * 引き出しと絵描きをかけます
     */
    template<typename ...Args>
    struct Drawer {
        static void draw(const Args &...) {
            static_assert(sizeof...(Args) == 0, "Drawer is not specialized for this type");
        };
    };

    template<typename... Args>
    concept is_drawable = requires(){
        {
        sizeof ...(Args) == 1
        ? not vsl::command::is_command<std::remove_cvref_t<std::tuple_element_t<0, std::tuple<Args...>>>>
        : true
        };
    };

    template<typename PhaseStream>
    struct DrawStream {
        using this_type = DrawStream<PhaseStream>;

        PhaseStream &phase_stream;

        template<typename ...Args>
        requires is_drawable<Args...>
        this_type &operator<<(Args &&... args);

        this_type &operator<<(std::shared_ptr<vsl::command::__Command> cmd);

        this_type &operator<<(vsl::command::is_command auto cmd);
    };

    template<typename PhaseStream>
    template<typename... Args>
    requires is_drawable<Args...>
    DrawStream<PhaseStream> &DrawStream<PhaseStream>::operator<<(Args &&... args) {
        Drawer<std::remove_cvref_t<Args>...>::draw(*this, std::forward<Args>(args)...);
        return *this;
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream> &DrawStream<PhaseStream>::operator<<(vsl::command::is_command auto cmd) {
        phase_stream << cmd;
        return *this;
    }

    template<typename PhaseStream>
    DrawStream<PhaseStream> &DrawStream<PhaseStream>::operator<<(std::shared_ptr<vsl::command::__Command> cmd) {
        phase_stream << cmd;
        return *this;
    }
}

#endif //AHO_ALL_DRAWER_H
