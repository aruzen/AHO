#include <VSL/define.hpp>
#include <VSL/utils/string.hpp>

#ifdef _MSC_VER
#include <cstdlib>
std::string fixed_get_env(const std::string& name) {
    char* value = nullptr;
    size_t len = 0;

    if (_dupenv_s(&value, &len, name.c_str()) == 0 && value != nullptr) {
        std::string result(value);
        free(value);   // ← 必須
        return result;
    }

    return "";
}
#endif

void default_logger(const char* str) {
		std::cout << str;
}

template<bool V>
consteval VSL_NAMESPACE::logger_t get_default_logger() {
	return V ? default_logger : nullptr;
}

std::string VSL_NAMESPACE::expand_environments(const std::string& input) {
    static const std::regex env_pattern(R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})");
    std::string result;
    std::sregex_iterator itr(input.begin(), input.end(), env_pattern);
    std::sregex_iterator end;
    size_t last_pos = 0;

    for (; itr != end; ++itr) {
        const auto& match = *itr;
        result.append(input, last_pos, match.position() - last_pos);
#ifndef _MSC_VER
        if (const char* value = std::getenv(match[1].str().c_str())) {
            result += value;
        }
#else
        if (auto value = fixed_get_env(match[1].str()); value != "") {
            result += value;
        }
#endif
        last_pos = match.position() + match.length();
    }
    result.append(input, last_pos, std::string::npos);
    return result;
}

VSL_NAMESPACE::logger_t VSL_NAMESPACE::logger = get_default_logger<VSL_NAMESPACE::validation>();
