#include "pch.h"
#include "define.h"
#include "debug.h"

void default_logger(const char* str) {
		std::cout << str;
}

VSL_NAMESPACE::logger_t VSL_NAMESPACE::logger = default_logger;