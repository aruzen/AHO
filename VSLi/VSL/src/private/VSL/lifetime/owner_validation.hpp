#pragma once
#include <VSL/lifetime/owner_access.hpp>
#include <stdexcept>

namespace VSL_NAMESPACE::lifetime::detail {
    // 所有の移動・回収と、新しい依存の公開に必要な生存検査を分ける。
    template<typename T>
    void require_live_owner(const OwnedRef<T>& owner) {
        const auto* node = OwnerAccess::Node(owner);
        if (node && node->retiring())
            throw std::invalid_argument("Resource owner is retiring");
    }
} // namespace VSL_NAMESPACE::lifetime::detail
