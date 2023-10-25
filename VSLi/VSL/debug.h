#pragma once
#include "define.h"

#include <string>
#include <iostream>
#include <sstream>

namespace VSL_NAMESPACE {
	constexpr bool validation = VSL_VALIDATION;

	typedef void (*logger_t)(const char*);
	extern logger_t logger;

	namespace helper {
		void join(std::stringstream& ss);
		template<typename Head, typename ...Args>
		void join(std::stringstream& ss, const Head& h, const Args&... args);
	}

	template<bool Validation = validation, typename Head, typename ...Args>
	void logging(const Head& h,const Args&... args);
	template<bool Validation = validation, typename Head, typename ...Args>
	void loggingln(const Head& h, const Args&... args);
}

inline void VSL_NAMESPACE::helper::join(std::stringstream& ss){}

template<typename Head, typename ...Args>
inline void VSL_NAMESPACE::helper::join(std::stringstream& ss, const Head& h, const Args & ...args)
{
	ss << h;
	VSL_NAMESPACE::helper::join(ss, args...);
}

template<bool Validation, typename Head, typename ...Args>
inline void VSL_NAMESPACE::logging(const Head& h, const Args & ...args)
{
	if (logger == nullptr)
		return;

	std::stringstream ss;
	VSL_NAMESPACE::helper::join(ss, h, args...);

	VSL_NAMESPACE::logger(ss.str().c_str());
}

template<bool Validation, typename Head, typename ...Args>
inline void VSL_NAMESPACE::loggingln(const Head& h, const Args & ...args)
{
	if (logger == nullptr)
		return;

	std::stringstream ss;
	VSL_NAMESPACE::helper::join(ss, h, args...);
	ss << std::endl;

	VSL_NAMESPACE::logger(ss.str().c_str());
}