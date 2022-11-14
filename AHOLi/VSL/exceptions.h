#pragma once
#include "define.h"

#include <string>

namespace VSL_NAMESPACE::exceptions {
	struct VSL_NAMESPACEException{
		std::string name, message, traceinfo;
	};

	struct RuntimeException : VSL_NAMESPACEException{
		RuntimeException(std::string type, std::string message = "", std::string traceinfo = "");
	};
}