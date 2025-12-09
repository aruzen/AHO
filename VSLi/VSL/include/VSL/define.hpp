#pragma once
#include "define.hpp"

// 
#define ___VSL_JOIN_S(x, y) x ## y
#define ___VSL_JOIN(x, y)___VSL_JOIN_S(x, y)


#define ___VSL_JOIN3_S(x, y, z) x ## y ## z
#define ___VSL_JOIN3(x, y, z)___VSL_JOIN3_S(x, y, z)
//

#define VSL_NAMESPACE vsl

#ifndef VSL_DEFAULT_dimension
#define VSL_DEFAULT_dimension 2
#endif // !VSL_DEFAULT_dimension

#define VSL_DEFAULT_dimension_STRUCT ___VSL_JOIN(D , VSL_DEFAULT_dimension)

#ifndef VSL_VALIDATION
#if defined(_DEBUG) || defined(__APPLE_CC__)
#define VSL_VALIDATION true
#else 
#define VSL_VALIDATION false
#endif
#endif // !VSL_DEFAULT_dimension

#define ___VSL_JOIN_S_2(s1, s2) s1 ## s2
#define ___VSL_JOIN_S_3(s1, s2, s3) s1 ## s2 ## s3
#define ___VSL_JOIN_S_4(s1, s2, s3, s4) s1 ## s2 ## s3 ## s4
#define ___VSL_JOIN_S_5(s1, s2, s3, s4, s5) s1 ## s2 ## s3 ## s4 ## s5
#define ___VSL_JOIN_S_6(s1, s2, s3, s4, s5, s6) s1 ## s2 ## s3 ## s4 ## s5 ## s6
#define ___VSL_JOIN_S_7(s1, s2, s3, s4, s5, s6, s7) s1 ## s2 ## s3 ## s4 ## s5 ## s6 ## s7
#define ___VSL_JOIN_S_8(s1, s2, s3, s4, s5, s6, s7, s8) s1 ## s2 ## s3 ## s4 ## s5 ## s6 ## s7 ## s8
#define ___VSL_JOIN_S_9(s1, s2, s3, s4, s5, s6, s7, s8, s9) s1 ## s2 ## s3 ## s4 ## s5 ## s6 ## s7 ## s8 ## s9

#define ___VSL_JOIN_2(s1, s2) ___VSL_JOIN_S_2(s1, s2)
#define ___VSL_JOIN_3(s1, s2, s3) ___VSL_JOIN_S_3(s1, s2, s3)
#define ___VSL_JOIN_4(s1, s2, s3, s4) ___VSL_JOIN_S_4(s1, s2, s3, s4)
#define ___VSL_JOIN_5(s1, s2, s3, s4, s5) ___VSL_JOIN_S_5(s1, s2, s3, s4, s5)
#define ___VSL_JOIN_6(s1, s2, s3, s4, s5, s6) ___VSL_JOIN_S_6(s1, s2, s3, s4, s5, s6)
#define ___VSL_JOIN_7(s1, s2, s3, s4, s5, s6, s7) ___VSL_JOIN_S_7(s1, s2, s3, s4, s5, s6, s7)
#define ___VSL_JOIN_8(s1, s2, s3, s4, s5, s6, s7, s8) ___VSL_JOIN_S_8(s1, s2, s3, s4, s5, s6, s7, s8)
#define ___VSL_JOIN_9(s1, s2, s3, s4, s5, s6, s7, s8, s9) ___VSL_JOIN_S_9(s1, s2, s3, s4, s5, s6, s7, s8, s9)