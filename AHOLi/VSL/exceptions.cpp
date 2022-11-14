#include "pch.h"
#include "define.h"
#include "exceptions.h"

VSL_NAMESPACE::exceptions::RuntimeException::RuntimeException(std::string type, std::string message, std::string traceinfo)
	: VSL_NAMESPACEException{ type == "" ? "RuntimeException" : "RuntimeException by " + type, message, traceinfo, } { }
