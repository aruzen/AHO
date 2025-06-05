//
// Created by morimoto_hibiki on 2025/06/03.
//

#include "defaults.hpp"

namespace VSL_NAMESPACE::defaults {
    std::shared_ptr<VSL_NAMESPACE::_impl::CommandManager_impl> COMMAND_MANAGER = nullptr;


    void release() {
        COMMAND_MANAGER = nullptr;
    }
}
