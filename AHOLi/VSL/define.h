#pragma once
#include "define.h"

// 
#define ___VSL_JOIN_S(x, y) x ## y
#define ___VSL_JOIN(x, y)___VSL_JOIN_S(x, y)


#define ___VSL_JOIN3_S(x, y, z) x ## y ## z
#define ___VSL_JOIN3(x, y, z)___VSL_JOIN3_S(x, y, z)
//

#define VSL_NAMESPACE vsl

#ifndef VSL_DEFAULT_DIMENTION
#define VSL_DEFAULT_DIMENTION 2
#endif // !VSL_DEFAULT_DIMENTION

#define VSL_DEFAULT_DIMENTION_STRUCT ___VSL_JOIN(D , VSL_DEFAULT_DIMENTION)

#ifndef VSL_VALIDATION
#ifdef  _DEBUG
#define VSL_VALIDATION true
#else 
#define VSL_VALIDATION false
#endif
#endif // !VSL_DEFAULT_DIMENTION