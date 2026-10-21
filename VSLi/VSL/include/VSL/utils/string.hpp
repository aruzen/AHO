#pragma once
#include "../define.hpp"

#include <string>
#include <iostream>
#include <sstream>
#include <regex>

namespace VSL_NAMESPACE {
    constexpr bool VALIDATION = VSL_VALIDATION;

    typedef void (*LoggerT)(const char*);
    extern LoggerT LOGGER;

    namespace helper {
        void join(std::stringstream& ss);
        template<typename Head, typename... Args>
        void join(std::stringstream& ss, const Head& h, const Args&... args);
    } // namespace helper

    template<bool validation = VALIDATION, typename Head, typename... Args>
    void logging(const Head& h, const Args&... args);
    template<bool validation = VALIDATION, typename Head, typename... Args>
    void loggingln(const Head& h, const Args&... args);

    std::string expand_environments(const std::string& input);
} // namespace VSL_NAMESPACE

inline void VSL_NAMESPACE::helper::join(std::stringstream& ss) {}

template<typename Head, typename... Args>
inline void VSL_NAMESPACE::helper::join(std::stringstream& ss, const Head& h, const Args&... args) {

    if constexpr (std::same_as<Head, bool>)
        ss << (h ? "true" : "false");
    else
        ss << h;
    VSL_NAMESPACE::helper::join(ss, args...);
}

template<bool validation, typename Head, typename... Args>
inline void VSL_NAMESPACE::logging(const Head& h, const Args&... args) {
    if constexpr (validation) {
        if (LOGGER == nullptr)
            return;

        std::stringstream ss;
        VSL_NAMESPACE::helper::join(ss, h, args...);

        VSL_NAMESPACE::LOGGER(ss.str().c_str());
    }
}

template<bool validation, typename Head, typename... Args>
inline void VSL_NAMESPACE::loggingln(const Head& h, const Args&... args) {
    if constexpr (validation) {
        if (LOGGER == nullptr)
            return;

        std::stringstream ss;
        VSL_NAMESPACE::helper::join(ss, h, args...);
        ss << std::endl;

        VSL_NAMESPACE::LOGGER(ss.str().c_str());
    }
}
