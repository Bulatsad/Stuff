#pragma once

#include <blib/config.h>

#ifdef WIN32 
#define __blib_force_inline __forceinline
#define __blib_noinline __declspec(noinline)
#elif defined(__GNUC__)
#define __blib_force_inline __attribute__((__always_inline__))
#define __blib_noinline __attribute__((__noinline__))
#else
#error "can not bind force inline specifiers"
#define __blib_force_inline
#define __blib_noinline
#endif

#ifndef __blib_inline
#ifdef __blib_inline_always
#define __blib_inline __blib_force_inline
#elif __blib_inline_never
#define __blib_inline __blib_noinline
#elif __blib_inline_compiler
#define __blib_inline inline
#else
#error "choose inline mode"
#endif 

#endif // !__blib_inline

#ifndef __blib_private_func
#define __blib_private_func static
#endif // !__blib___blib_private_func




