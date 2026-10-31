#pragma once
#include "../define.hpp"
#include <memory>

namespace VSL_NAMESPACE::storage {
#if defined(_LIBCPP_ABI_VERSION)
    inline constexpr size_t BUFFER_CPP_ABI_MODE = 10000 + _LIBCPP_ABI_VERSION * 100;
#elif defined(_GLIBCXX_USE_CXX11_ABI)
#ifdef _GLIBCXX_DEBUG
    inline constexpr size_t BUFFER_CPP_ABI_MODE = 20010 + _GLIBCXX_USE_CXX11_ABI;
#else
    inline constexpr size_t BUFFER_CPP_ABI_MODE = 20000 + _GLIBCXX_USE_CXX11_ABI;
#endif
#elif defined(_ITERATOR_DEBUG_LEVEL)
    inline constexpr size_t BUFFER_CPP_ABI_MODE = 30000 + _ITERATOR_DEBUG_LEVEL;
#else
    inline constexpr size_t BUFFER_CPP_ABI_MODE = 0;
#endif
} // namespace VSL_NAMESPACE::storage
