#pragma once
#include <VSL/define.hpp>
#include <VSL/exceptions.hpp>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace VSL_NAMESPACE::_impl::helper {
    constexpr std::uint32_t checkedDrawCount(std::size_t count) {
        if (!std::in_range<std::uint32_t>(count))
            throw exceptions::RuntimeException("DrawCount", "Draw count exceeds uint32_t range");
        return static_cast<std::uint32_t>(count);
    }
} // namespace VSL_NAMESPACE::_impl::helper
