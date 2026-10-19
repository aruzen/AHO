#pragma once
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace vsl_test {
class ScopedEnvironment {
    std::string name_;
    std::optional<std::string> previous_;

  public:
    ScopedEnvironment(std::string name, const char *value) : name_(std::move(name)) {
        if (const auto *previous = std::getenv(name_.c_str()))
            previous_ = previous;
        set(value);
    }
    ~ScopedEnvironment() {
        set(previous_ ? previous_->c_str() : nullptr);
    }
    void set(const char *value) const {
#ifdef _WIN32
        _putenv_s(name_.c_str(), value ? value : "");
#else
        if (value)
            setenv(name_.c_str(), value, 1);
        else
            unsetenv(name_.c_str());
#endif
    }
};
} // namespace vsl_test
