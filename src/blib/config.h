#pragma once

#define __blib_default_cache_size 64

#define __blib_platform_depended

#if ____blib_configuration_platform_value == ____blib_configuration_platform_undefined
//#error "Compile platform must be defined"
#elif ____blib_configuration_platform_value == ____blib_configuration_platform_windows
#define __blib_compile_platform_windows
#else
//#error "Unsupported compile platform"
#endif //____blib_configuration_platform_value

#ifdef blib_import

#ifdef blib_dynamic
#define __blib_api __declspec(dllimport)                                                    // dynamic
#endif 

#ifdef blib_static
#define __blib_api                                                                          // static 
#endif 

#ifndef __blib_api
#error "Unknown link type"
#endif // !__blib_api

#define __blib_include_implementation(_include)

#endif // blib_import

#ifdef blib_export

#ifdef __blib_compile_platform_windows
#if ____blib_configuration_library_type_value == ____blib_configuration_library_type_shared 
#define __blib_api __declspec(dllexport)                                                      // dynamic
#elif ____blib_configuration_library_type_value == ____blib_configuration_library_type_static 
#define __blib_api                                                                            // static
#else
#error "Unknow build type"
#endif                           
#endif // __blib_compile_platform_windows

#define __blib_include_implementation(_include) _include

#endif // blib_export

#ifndef __blib_unsafe
#define __blib_unsafe
#endif // !__blib_unsafe


//Current render api
#define __blib_render_api_opengl

