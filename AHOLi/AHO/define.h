#pragma once

#ifndef AHO_NAMESPACE
#define AHO_NAMESPACE aho
#endif

#ifndef AHO_COORDINATE_NUMBERD_MEMBER_NAME
#define AHO_COORDINATE_NUMBERD_MEMBER_NAME _cnmn
#endif

// #define AHO_POOP_PUBLIC_SECURITY

#define ___AHO_JOIN_S(x, y) x ## y
#define ___AHO_JOIN(x, y) ___AHO_JOIN_S(x, y)


#define ___AHO_JOIN3_S(x, y, z) x ## y ## z
#define ___AHO_JOIN3(x, y, z) ___AHO_JOIN3_S(x, y, z)

#ifndef AHO_POOP_PUBLIC_SECURITY 

#define AHO_LITERAL(suffix) ___AHO_JOIN(""_, suffix)

#else

#define AHO_LITERAL(suffix) ___AHO_JOIN("", suffix)

#endif