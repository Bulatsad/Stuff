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

// В shared-сборке Windows НЕ используем dllimport у потребителей:
// многие классы blib имеют inline-члены (vtable/деструкторы в хедерах),
// и dllimport-объявления искали бы их импорты, которых в DLL нет.
// Вместо этого экспортирует ТОЛЬКО сам модуль (blib_*_export → dllexport),
// а потребители ссылаются напрямую — линкер MSVC резолвит их через
// импорт-таблицу (auto-import функций и данных из экспортированной DLL;
// всё публичное покрыто WINDOWS_EXPORT_ALL_SYMBOLS).
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

// Static data members dll-класса НЕ наследуют атрибут класса (MSVC),
// а линкер без dllimport данные не импортирует. Поэтому для ДАННЫХ
// отдельный макрос: dllimport у потребителей (импорт-таблица содержит
// __imp_ от авто-экспорта), пусто при сборке самого модуля (класс с
// dllexport уже экспортирует данные; повторный dllexport на члене — C2487).
#if defined(blib_system_export) || defined(blib_core_export) || defined(blib_graphics_export) \
    || defined(blib_sound_export) || defined(blib_network_export)
#define __blib_data_api
#else
#define __blib_data_api __declspec(dllimport)
#endif // внутри модуля blib

#endif // shared
#endif // __blib_compile_platform_windows

// Вне shared-сборки Windows данные не декорируются
#ifndef __blib_data_api
#define __blib_data_api
#endif

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

// ---------------------------------------------------------------
// Макрос объявления чисто виртуальной функции (вместо "= 0"):
//   virtual void foo() __blib_pure_virtual_function;
#define __blib_pure_virtual_function = 0
