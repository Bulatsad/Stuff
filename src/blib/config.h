#pragma once

#define __blib_default_cache_size 64

#define __blib_max_bones 100

#define __blib_platform_depended

// ---------------------------------------------------------------
// BLIB_DEBUG — признак debug-сборки: включает debug-ветки кода
// и DebugAllocator в аллокаторах (см. defaultAllocator.h и др.).
// Источник признака:
//   - MSVC: стандартный макрос _DEBUG (компилятор задаёт его сам
//     в Debug-конфигурации);
//   - GCC/Clang: флаг -DBLIB_DEBUG, который добавляется в
//     CMAKE_CXX_FLAGS_DEBUG в blib/CMakeLists.txt.
#if defined(_DEBUG) || defined(BLIB_DEBUG)
#ifndef BLIB_DEBUG
#define BLIB_DEBUG
#endif
#endif // _DEBUG || BLIB_DEBUG

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

// ---------------------------------------------------------------
// Branch prediction hints для обработки ошибок.
// Помогают компилятору оптимизировать hot path, вынося холодные
// ветки (обработка ошибок) в конец функции.
// 
// Использование:
//   if (__blib_expect_false(error_condition)) {
//       // обработка ошибки (cold path)
//   }
//   // hot path продолжается
//
// Поддержка:
//   - GCC/Clang: __builtin_expect (работает с C++11+)
//   - MSVC: компилятор игнорирует, но код компилируется
//   - Другие: fallback без оптимизации
#if defined(__GNUC__) || defined(__clang__)
    #define __blib_expect_true(x)  __builtin_expect(!!(x), 1)
    #define __blib_expect_false(x) __builtin_expect(!!(x), 0)
#else
    // MSVC и другие компиляторы — без оптимизации
    #define __blib_expect_true(x)  (x)
    #define __blib_expect_false(x) (x)
#endif

// Краткие алиасы для удобства
#define __blib_likely(x)   __blib_expect_true(x)
#define __blib_unlikely(x) __blib_expect_false(x)

// ---------------------------------------------------------------
// Обработка ошибок: макросы возврата и фатальных ошибок
// (архитектура: ERROR_HANDLING_ARCHITECTURE.md в корне репозитория).
//
// __blib_return_error(code, ...) — логирует ошибку в Console и
// возвращает код из функции. Требует включённый
// <blib/core/console/console.h> в месте использования.
//
// __blib_fatal(...) — логирует фатальную ошибку и аварийно
// завершает процесс через std::abort(). Требует <cstdlib>
// и <blib/core/console/console.h> в месте использования.
#define __blib_return_error(code, ...) \
    do { \
        __blib_log_error(__VA_ARGS__); \
        return code; \
    } while(0)

#define __blib_fatal(...) \
    do { \
        __blib_log_error(__VA_ARGS__); \
        std::abort(); \
    } while(0)
