#ifdef _MSC_VER
#include "pch.h"
#endif
#include <VSL/define.hpp>
#include <VSL/exceptions.hpp>

const char* VSL_NAMESPACE::exceptions::VSLException::what() {
	return reason.c_str();
}

VSL_NAMESPACE::exceptions::RuntimeException::RuntimeException(std::string type, std::string message, std::string traceinfo){
    reason = (type == "" ? "RuntimeException" : "RuntimeException by " + type) + " : " + message + "(" + traceinfo + ")";
}


VSL_NAMESPACE::exceptions::RuntimeException::RuntimeException(std::string type, std::string message, std::source_location source) {
    reason = (type == "" ? "RuntimeException" : "RuntimeException by " + type) + " : " + message + "(" + std::to_string(source.line()) + ")";
}
