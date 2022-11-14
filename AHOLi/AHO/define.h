#pragma once

#define AHO_NAMESPACE aho

#define AHO_POOP_PUBLIC_SECURITY

#define ___AHO_JOIN_S(x, y) x ## y
#define ___AHO_JOIN(x, y)___AHO_JOIN_S(x, y)


#define ___AHO_JOIN3_S(x, y, z) x ## y ## z
#define ___AHO_JOIN3(x, y, z)___AHO_JOIN3_S(x, y, z)

#ifndef AHO_POOP_PUBLIC_SECURITY 

#define AHO_LITERAL(suffix) ___AHO_JOIN(_, suffix)

#else

#define AHO_LITERAL(suffix) suffix

#endif