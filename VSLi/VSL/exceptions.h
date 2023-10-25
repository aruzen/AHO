#pragma once
#include "define.h"

#include <string>
#include <source_location>

namespace VSL_NAMESPACE::exceptions {
	struct VSLException{
		std::string name, message, traceinfo;

		std::string what();
	};

	struct RuntimeException : VSLException{
		RuntimeException(std::string type, std::string message = "", std::string traceinfo = "");
		RuntimeException(std::string type, std::string message, std::source_location sourece);
	};
}