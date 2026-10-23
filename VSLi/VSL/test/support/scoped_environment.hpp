#pragma once
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace vsl_test {
    class ScopedEnvironment {
        std::string _name;
        std::optional<std::string> _previous;

    public:
        ScopedEnvironment(std::string name, const char* value) : _name(std::move(name)) {
            if (const auto* previous = std::getenv(_name.c_str()))
                _previous = previous;
            set(value);
        }
        ~ScopedEnvironment() {
            set(_previous ? _previous->c_str() : nullptr);
        }
        void set(const char* value) const;
    };

    inline void ScopedEnvironment::set(const char* value) const {
#ifdef _WIN32
        _putenv_s(_name.c_str(), value ? value : "");
#else
        if (value)
            setenv(_name.c_str(), value, 1);
        else
            unsetenv(_name.c_str());
#endif
    }
} // namespace vsl_test
