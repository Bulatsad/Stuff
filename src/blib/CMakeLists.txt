#blib — хаб модулей: общие опции, общая конфигурация и подключение модулей
#Модули:
#  blib-system   — платформенные примитивы (thread, memory) и базовые заголовки
#  blib-core     — переносимое ядро (потоки, строки, algorithm, math, pdl)
#  blib-graphics — рендер OpenGL + Dear ImGui (только Windows)
#  blib-sound    — звук (WinMM, только Windows)
#  blib-network  — сеть (winsock, только Windows)

set (BIN blib)

project(${BIN})

set(blib_build_type_enum blib_build_static blib_build_dynamic)
set(blib_build_type blib_build_static CACHE STRING "Library build type")
set_property(CACHE blib_build_type PROPERTY STRINGS ${blib_build_type_enum})

set(blib_inline_mode_enum __blib_inline_always __blib_inline_never __blib_inline_compiler)
set(blib_inline_mode __blib_inline_compiler CACHE STRING "Inline mode")
set_property(CACHE blib_inline_mode PROPERTY STRINGS ${blib_inline_mode_enum})

# Числительные значения конфигурации (читаются из blib/config.h)
set(____blib_configuration_platform_undefined 0)
set(____blib_configuration_platform_windows 1)
set(____blib_configuration_platform_linux 2)
set(____blib_configuration_platform_macos 3)

set(____blib_configuration_library_type_undefined 0)
set(____blib_configuration_library_type_static 1)
set(____blib_configuration_library_type_shared 2)

# Выбор платформы
if(WIN32)
    set(____blib_configuration_platform_value ${____blib_configuration_platform_windows})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(____blib_configuration_platform_value ${____blib_configuration_platform_linux})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(____blib_configuration_platform_value ${____blib_configuration_platform_macos})
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Выбор типа библиотеки
if(blib_build_type STREQUAL "blib_build_static")
    set(____blib_configuration_library_type_value ${____blib_configuration_library_type_static})
elseif(blib_build_type STREQUAL "blib_build_dynamic")
    set(____blib_configuration_library_type_value ${____blib_configuration_library_type_shared})
else()
    message(FATAL_ERROR "Undefined build type")
endif()

option(COMPILE_ASSIMP_COMPATIBLE "Enable Assimp compatibility (loadFromAssimp, Assimp headers)" ON)

# Модули, у которых реализация есть только под Windows, по умолчанию
# собираются только на Windows (на остальных платформах — опционально выключены)
if(WIN32)
    option(BLIB_BUILD_GRAPHICS "Build blib-graphics module" ON)
    option(BLIB_BUILD_SOUND "Build blib-sound module" ON)
    option(BLIB_BUILD_NETWORK "Build blib-network module" ON)
else()
    option(BLIB_BUILD_GRAPHICS "Build blib-graphics module" OFF)
    option(BLIB_BUILD_SOUND "Build blib-sound module" OFF)
    option(BLIB_BUILD_NETWORK "Build blib-network module" OFF)
endif()

# Корень инклюдов: все инклюды в проекте имеют вид <blib/...>, корень — src/
set(blib_include_root "${CMAKE_CURRENT_SOURCE_DIR}/../")

# ---------------------------------------------------------------
# Хелперы

# Создаёт модульный таргет в соответствии с blib_build_type.
# Все модули кладутся в папку "blib" Solution Explorer.
function(blib_add_module target_name)
    if(blib_build_type STREQUAL "blib_build_static")
        add_library(${target_name} STATIC ${ARGN})
    elseif(blib_build_type STREQUAL "blib_build_dynamic")
        add_library(${target_name} SHARED ${ARGN})
    else()
        message(FATAL_ERROR "Undefined build type")
    endif()

    set_target_properties(${target_name} PROPERTIES FOLDER "blib" LINKER_LANGUAGE CXX)
endfunction()

# Общая конфигурация модульного таргета: дефайны конфигурации (PUBLIC —
# они нужны и потребителям, т.к. blib/config.h читает их в каждом TU),
# inline-режим, export-макрос модуля и корень инклюдов <blib/...>.
function(blib_configure_module target_name export_define)
    target_compile_definitions(${target_name} PUBLIC
        ____blib_configuration_platform_undefined=${____blib_configuration_platform_undefined}
        ____blib_configuration_platform_windows=${____blib_configuration_platform_windows}
        ____blib_configuration_platform_linux=${____blib_configuration_platform_linux}
        ____blib_configuration_platform_macos=${____blib_configuration_platform_macos}
        ____blib_configuration_platform_value=${____blib_configuration_platform_value}

        ____blib_configuration_library_type_undefined=${____blib_configuration_library_type_undefined}
        ____blib_configuration_library_type_static=${____blib_configuration_library_type_static}
        ____blib_configuration_library_type_shared=${____blib_configuration_library_type_shared}
        ____blib_configuration_library_type_value=${____blib_configuration_library_type_value}

        ${export_define}
    )

    if(blib_inline_mode STREQUAL "__blib_inline_always")
        target_compile_definitions(${target_name} PRIVATE __blib_inline_always)
    elseif(blib_inline_mode STREQUAL "__blib_inline_never")
        target_compile_definitions(${target_name} PRIVATE __blib_inline_never)
    elseif(blib_inline_mode STREQUAL "__blib_inline_compiler")
        target_compile_definitions(${target_name} PRIVATE __blib_inline_compiler)
    else()
        message(FATAL_ERROR "Undefined inline mode")
    endif()

    # Все инклюды в проекте имеют вид <blib/...>, корень — src/
    target_include_directories(${target_name} PUBLIC "${blib_include_root}")
endfunction()

# MSVC-флаги проекта (действуют на blib и его подкаталоги, включая test)
if(MSVC)
    set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )

    message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
endif()

# ---------------------------------------------------------------
# Модули
add_subdirectory(system)
add_subdirectory(core)

if(BLIB_BUILD_GRAPHICS)
    if(NOT WIN32)
        message(FATAL_ERROR "blib-graphics has no non-Windows implementation yet")
    endif()
    add_subdirectory(graphics)
endif()

if(BLIB_BUILD_SOUND)
    if(NOT WIN32)
        message(FATAL_ERROR "blib-sound has no non-Windows implementation yet")
    endif()
    add_subdirectory(sound)
endif()

if(BLIB_BUILD_NETWORK)
    if(NOT WIN32)
        message(FATAL_ERROR "blib-network has no non-Windows implementation yet")
    endif()
    add_subdirectory(network)
endif()

#tests
if(BUILD_TESTS)
	add_subdirectory(test)
endif()
