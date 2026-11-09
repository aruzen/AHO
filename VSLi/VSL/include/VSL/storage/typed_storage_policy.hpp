#pragma once
#include "../define.hpp"

namespace VSL_NAMESPACE::typed {
    template<typename... Bindings>
    struct ResourceBindingMap;
    struct HeapStorage {};
    template<typename Bindings_ = ResourceBindingMap<>>
    struct ArenaStorage {
        using Bindings = Bindings_;
    };
    using DefaultStorage = ArenaStorage<>;
} // namespace VSL_NAMESPACE::typed
