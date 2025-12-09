#pragma once
#include "define.hpp"

#include <string>
#include <source_location>
#include <exception>

namespace VSL_NAMESPACE::exceptions {
	struct VSLException : public std::exception {
		std::string reason;

        virtual const char* what();
	};

	struct RuntimeException : public VSLException{
		RuntimeException(std::string type, std::string message = "", std::string traceinfo = "");
		RuntimeException(std::string type, std::string message, std::source_location sourece);
	};
}