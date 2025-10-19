#ifdef _MSC_VER
#include "pch.h"
#endif
#include "../define.h"
#include "string.h"

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
        if (const char* value = std::getenv(match[1].str().c_str())) {
            result += value;
        }
        last_pos = match.position() + match.length();
    }
    result.append(input, last_pos, std::string::npos);
    return result;
}

VSL_NAMESPACE::logger_t VSL_NAMESPACE::logger = get_default_logger<VSL_NAMESPACE::validation>();
