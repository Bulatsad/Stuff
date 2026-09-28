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

set_target_properties(gravelands-plugin PROPERTIES FOLDER "gravelands/plugin" LINKER_LANGUAGE CXX)

# Инклюд-корень game-слоя: src/misc — все инклюды игры имеют вид <gravelands/...>
target_include_directories(gravelands-plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# gravelands-world — мир; beng-editor-core — каркас EditorApplication;
# blib-graphics — registerGraphicsConsoleCommands (hot-reload шейдеров)
target_link_libraries(gravelands-plugin PUBLIC gravelands-world beng-editor-core blib-graphics)
