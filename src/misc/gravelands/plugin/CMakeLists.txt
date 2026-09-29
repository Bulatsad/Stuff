# gravelands-plugin — «игровая сторона» единого эдитора (плагин-модель,
# см. ARCHITECTURE.md): GravelandsEditorHost поверх каркаса
# EditorApplication + фабрика gravelandsCreateEditorHost() —
# единственная точка входа, на DLL-этапе она станет экспортируемым
# символом gravelands.dll.
#
# gravelands_plugin_type:
#   STATIC (дефолт) — плагин линкуется в beng-editor.exe: удобная
#     отладка (символы, точки останова, без DLL-путей); меняется
#     только способ доставки, код плагина — один и тот же;
#   SHARED — DLL-этап (потребует shared-сборки blib/beng-core и
#     dllexport — см. ARCHITECTURE.md, «Сложности плагин-модели»).

set(gravelands_plugin_type "STATIC" CACHE STRING "Gravelands editor plugin link type (STATIC | SHARED)")
set_property(CACHE gravelands_plugin_type PROPERTY STRINGS STATIC SHARED)

file (GLOB plugin_header_src "*.h")
file (GLOB plugin_impl_src "*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/plugin" "${plugin_header_src}")
GroupSoureces("${SourceGroupPrefix}/plugin" "${plugin_impl_src}")

add_library(gravelands-plugin ${gravelands_plugin_type}
    ${plugin_header_src}
    ${plugin_impl_src}
)

# DLL-режим: фабрики помечаются dllexport (GRAVELANDS_PLUGIN_EXPORTS) —
# единственные экспортируемые символы плагина; эдитор берёт их через
# GetProcAddress. Статический режим — макрос пустой
if(gravelands_plugin_type STREQUAL "SHARED")
    target_compile_definitions(gravelands-plugin PRIVATE GRAVELANDS_PLUGIN_EXPORTS)
endif()

set_target_properties(gravelands-plugin PROPERTIES FOLDER "gravelands/plugin" LINKER_LANGUAGE CXX)

# Инклюд-корень game-слоя: src/misc — все инклюды игры имеют вид <gravelands/...>
target_include_directories(gravelands-plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# gravelands-world — мир; beng-editor-core — каркас EditorApplication;
# gravelands-server-core — сервер (PIE); gravelands-client-core — клиент
# (PIE, отдельное окно); blib-graphics — registerGraphicsConsoleCommands
# (hot-reload шейдеров)
target_link_libraries(gravelands-plugin PUBLIC
    gravelands-world gravelands-server-core gravelands-client-core
    beng-editor-core blib-graphics)

# DLL-режим: gravelands.dll копируется рядом с beng-editor.exe —
# эдитор грузит её по имени (см. beng/editor/main/main.cpp)
if(gravelands_plugin_type STREQUAL "SHARED" AND TARGET beng-editor)
    add_custom_command(TARGET gravelands-plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:gravelands-plugin>
            $<TARGET_FILE_DIR:beng-editor>)
endif()
