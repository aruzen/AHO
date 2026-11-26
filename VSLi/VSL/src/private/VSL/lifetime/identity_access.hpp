#pragma once
#include <VSL/lifetime/resource_identity.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct IdentityAccess {
        template<typename Identity>
        static Identity From(const void* value) { return Identity(value); }
    };
} // namespace VSL_NAMESPACE::lifetime::detail
