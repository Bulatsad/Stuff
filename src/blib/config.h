#pragma once

#define __blib_default_cache_size 64

#define __blib_max_bones 100

#define __blib_platform_depended

// ---------------------------------------------------------------
// Платформа. Значение задаёт CMake через дефайн
// ____blib_configuration_platform_value (числительные значения
// ____blib_configuration_platform_* тоже приходят из CMake).
#if ____blib_configuration_platform_value == ____blib_configuration_platform_undefined
//#error "Compile platform must be defined"
#elif ____blib_configuration_platform_value == ____blib_configuration_platform_windows
#define __blib_compile_platform_windows
#elif ____blib_configuration_platform_value == ____blib_configuration_platform_linux
#define __blib_compile_platform_linux
#elif ____blib_configuration_platform_value == ____blib_configuration_platform_macos
#define __blib_compile_platform_macos
#else
//#error "Unsupported compile platform"
#endif //____blib_configuration_platform_value

// ---------------------------------------------------------------
// Модульные api-макросы. По умолчанию пустые: экспорт/импорт нужен
// только в shared-сборке на Windows, в остальных случаях символы
// не декорируются.
#define __blib_system_api
#define __blib_core_api
#define __blib_graphics_api
#define __blib_sound_api
#define __blib_network_api

#ifdef __blib_compile_platform_windows
#if ____blib_configuration_library_type_value == ____blib_configuration_library_type_shared

// В shared-сборке по умолчанию импортируем символы всех модулей,
// а текущий компилируемый модуль (его CMake задаёт blib_*_export)
// экспортирует свои.
#undef __blib_system_api
#undef __blib_core_api
#undef __blib_graphics_api
#undef __blib_sound_api
#undef __blib_network_api

#define __blib_system_api __declspec(dllimport)
#define __blib_core_api __declspec(dllimport)
#define __blib_graphics_api __declspec(dllimport)
#define __blib_sound_api __declspec(dllimport)
#define __blib_network_api __declspec(dllimport)

#ifdef blib_system_export
#undef __blib_system_api
#define __blib_system_api __declspec(dllexport)
#endif // blib_system_export

#ifdef blib_core_export
#undef __blib_core_api
#define __blib_core_api __declspec(dllexport)
#endif // blib_core_export

#ifdef blib_graphics_export
#undef __blib_graphics_api
#define __blib_graphics_api __declspec(dllexport)
#endif // blib_graphics_export

#ifdef blib_sound_export
#undef __blib_sound_api
#define __blib_sound_api __declspec(dllexport)
#endif // blib_sound_export

#ifdef blib_network_export
#undef __blib_network_api
#define __blib_network_api __declspec(dllexport)
#endif // blib_network_export

#endif // shared
#endif // __blib_compile_platform_windows

#ifndef __blib_unsafe
#define __blib_unsafe
#endif // !__blib_unsafe


//Current render api
#define __blib_render_api_opengl
