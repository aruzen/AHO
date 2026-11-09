#pragma once
#include "../define.hpp"
#include <concepts>
#include <type_traits>

namespace VSL_NAMESPACE::typed {
    template<typename... Bindings>
    struct ResourceBindingMap;
    struct HeapStorage {};
    template<typename Bindings_ = ResourceBindingMap<>>
    struct ArenaStorage {
        using Bindings = Bindings_;
    };
    using DefaultStorage = ArenaStorage<>;
    template<typename Policy>
    concept storage_policy = std::same_as<std::remove_cvref_t<Policy>, HeapStorage> || std::same_as<std::remove_cvref_t<Policy>, ArenaStorage<>>;
} // namespace VSL_NAMESPACE::typed
