#ifdef _MSC_VER
#include "pch.h"
#endif
#include "define.h"
#include "exceptions.h"

std::string VSL_NAMESPACE::exceptions::VSLException::what() {
	return name + " : " + message + "(" + traceinfo + ")";
}

VSL_NAMESPACE::exceptions::RuntimeException::RuntimeException(std::string type, std::string message, std::string traceinfo)
	: VSLException{ type == "" ? "RuntimeException" : "RuntimeException by " + type, message, traceinfo, } { }


VSL_NAMESPACE::exceptions::RuntimeException::RuntimeException(std::string type, std::string message, std::source_location source)
	: VSLException{ type == "" ? "RuntimeException" : "RuntimeException by " + type, message,
	std::string(source.file_name()) + " : " + std::string(source.function_name()) + " : " + std::to_string(source.line()) } { }
