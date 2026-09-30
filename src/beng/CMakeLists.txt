#beng-core — Bulat Engine: переносимое ядро движка (ECS: Scene, Component,
#System, Transform; Time; репликация-кодек). Ни графики, ни сети, ни звука.
#Зависит только от blib-core (blib-system транзитивно).
#beng-client/beng-editor-core собираются из корневого CMake (при blib-graphics),
#beng-server — при blib-network (см. server/CMakeLists.txt).
#
#Тип сборки — как у blib: beng_build_type = beng_build_static (дефолт) |
#beng_build_dynamic (shared-сборка для плагин-модели эдитора, см.
#ARCHITECTURE.md «Эдитор»: один экземпляр beng-кода на процесс). Один тип
#на все beng-модули (core/client/editor-core) — как blib_build_type у blib.

set(beng_build_type_enum beng_build_static beng_build_dynamic)
set(beng_build_type beng_build_static CACHE STRING "beng library build type")
set_property(CACHE beng_build_type PROPERTY STRINGS ${beng_build_type_enum})

# Хелперы модульных таргетов (паттерн blib/CMakeLists.txt)

function(beng_add_module target_name)
    if(beng_build_type STREQUAL "beng_build_static")
        add_library(${target_name} STATIC ${ARGN})
    elseif(beng_build_type STREQUAL "beng_build_dynamic")
        add_library(${target_name} SHARED ${ARGN})
        # Авто-экспорт всех символов DLL (MSVC): покрывает template-инстанции
        # (ComponentPool<T> и т.п.) и классы без __beng_api
        set_target_properties(${target_name} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    else()
        message(FATAL_ERROR "Undefined beng build type")
    endif()

    set_target_properties(${target_name} PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)
endfunction()

# Экспорт-дефайн модуля: в shared-сборке модуль-библиотека экспортирует
# свои символы (beng_export → __beng_api = dllexport) ТОЛЬКО при
# компиляции себя (PRIVATE); потребители остаются на dllimport.
# Конфигурационные дефайны ____blib_configuration_* приходят транзитивно
# из blib-core (PUBLIC)
function(beng_configure_module target_name export_define)
    target_compile_definitions(${target_name} PRIVATE ${export_define})
endfunction()

file (GLOB core_header_src "core/*.h" "core/impl/*.inl")
file (GLOB core_impl_src "core/*.cpp")

file (GLOB component_header_src "components/*.h")
file (GLOB component_impl_src "components/*.cpp")

file (GLOB system_header_src "systems/*.h")
file (GLOB system_impl_src "systems/*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
# (иначе Visual Studio показывает только .cpp)
GroupSoureces("${HeaderGroupPrefix}/core" "${core_header_src}")
GroupSoureces("${SourceGroupPrefix}/core" "${core_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/components" "${component_header_src}")
GroupSoureces("${SourceGroupPrefix}/components" "${component_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/systems" "${system_header_src}")
GroupSoureces("${SourceGroupPrefix}/systems" "${system_impl_src}")

GroupSoureces("${HeaderGroupPrefix}" "config.h")

# Пока beng-core собирается как static или shared (beng_build_type)
beng_add_module(beng-core
    config.h

    ${core_header_src}
    ${core_impl_src}

    ${component_header_src}
    ${component_impl_src}

    ${system_header_src}
    ${system_impl_src}
)

beng_configure_module(beng-core beng_export)

set_target_properties(beng-core PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/...>, корень — src/
target_include_directories(beng-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# blib-core передаёт blib-system и все конфигурационные дефайны (PUBLIC)
target_link_libraries(beng-core PUBLIC blib-core)
