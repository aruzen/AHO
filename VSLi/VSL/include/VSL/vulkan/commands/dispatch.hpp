//
// Created by morimoto_hibiki on 2025/07/31.
//

#ifndef AHO_ALL_DISPATCH_HPP
#define AHO_ALL_DISPATCH_HPP

#include "../../define.hpp"
#include "../pv.hpp"
#include "../command.hpp"

#include "../pipeline.hpp"

namespace VSL_NAMESPACE::command {
    struct Dispatch : public __Command {
        Dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z);

        std::uint32_t x, y, z;

        void invoke(CommandPool pool, CommandBuffer buffer, CommandManager manager);
    };
}


#endif //AHO_ALL_DISPATCH_HPP
