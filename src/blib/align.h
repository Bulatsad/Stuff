#pragma once

#include <new>

#include <blib/config.h>

#ifdef __cpp_lib_hardware_interference_size
#define __blib_cache_size std::hardware_destructive_interference_size
#else
#ifdef __blib_default_cache_size
#define __blib_cache_size ((size_t)__blib_default_cache_size)
#else
#error "__cpp_lib_hardware_interference_size and __blib_default_cache_size is not defined current machine cache line size is unknown"
#endif // __blib_default_cache_size
#endif // __cpp_lib_hardware_interference_size

#ifdef WIN32
#define __blib_align(__a_align) __declspec(align(__a_align))
#else
#error "can not define align command for current compiler"
#endif // _WIN

#ifdef __blib_align
#define __blib_cache_aligned __blib_align(__blib_cache_size)
#else
#error "align macro is not defined"
#endif // __blib_align

#ifndef __blib_thread_safe
#define __blib_thread_safe
#endif // !__blib_thread_safe
