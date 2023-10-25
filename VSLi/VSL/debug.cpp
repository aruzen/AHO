#include "pch.h"
#include "define.h"
#include "debug.h"

void default_logger(const char* str) {
		std::cout << str;
}

template<bool V>
consteval VSL_NAMESPACE::logger_t get_default_logger() {
	return V ? default_logger : nullptr;
}

VSL_NAMESPACE::logger_t VSL_NAMESPACE::logger = get_default_logger<VSL_NAMESPACE::validation>();