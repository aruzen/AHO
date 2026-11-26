#pragma once
#include "resource_use.hpp"

namespace VSL_NAMESPACE::lifetime {
    // 他の資源を所有しない独立した論理状態。arenaの構築時依存として宣言できる。
    class LogicalUseState {
        std::shared_ptr<ResourceUseState> _state;

    public:
        LogicalUseState();
        ResourceUse resource_use() const;
        bool busy() const;
        bool wait(std::uint64_t timeout = UINT64_MAX) const;
    };
} // namespace VSL_NAMESPACE::lifetime
