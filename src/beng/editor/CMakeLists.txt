#beng-editor-core — каркас эдитора (EditorApplication: окно, FBO,
#камера, ImGui, сцена, раскладка) + панели на Dear ImGui (Hierarchy,
#анимации, вьюпорт, опции рендера, консоль). Панели реализуют
#IPanel и не зависят от конкретного приложения — их можно
#переиспользовать в любом инструменте на базе beng.
#beng-editor — ЕДИНЫЙ exe эдитора на все игры (плагин-модель, см.
#ARCHITECTURE.md): тонкая main() над beng-editor-core.
#Добавляется из корневого CMake при наличии blib-graphics.

file (GLOB panel_header_src "panels/*.h")
file (GLOB panel_impl_src "panels/*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/editor" "${CMAKE_CURRENT_SOURCE_DIR}/editorApplication.h")
GroupSoureces("${HeaderGroupPrefix}/editor" "${CMAKE_CURRENT_SOURCE_DIR}/editorIcons.h")
GroupSoureces("${SourceGroupPrefix}/editor" "${CMAKE_CURRENT_SOURCE_DIR}/editorApplication.cpp")
GroupSoureces("${HeaderGroupPrefix}/editor/panels" "${panel_header_src}")
GroupSoureces("${SourceGroupPrefix}/editor/panels" "${panel_impl_src}")
GroupSoureces("${SourceGroupPrefix}/editor/main" "${CMAKE_CURRENT_SOURCE_DIR}/main/main.cpp")

beng_add_module(beng-editor-core
    ${CMAKE_CURRENT_SOURCE_DIR}/editorApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/editorIcons.h
    ${CMAKE_CURRENT_SOURCE_DIR}/editorApplication.cpp

    ${panel_header_src}
    ${panel_impl_src}
)

beng_configure_module(beng-editor-core beng_export)

set_target_properties(beng-editor-core PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/editor/...>, корень — src/beng
target_include_directories(beng-editor-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# beng-client даёт компоненты (AnimatorComponent в панели анимаций),
# blib-graphics — ImGui-хедеры, ConsoleWindow, OrbitCamera
target_link_libraries(beng-editor-core PUBLIC beng-client)

# Comdlg32 — Win32-файлдиалог (Edit → Load Font... в меню-баре каркаса)
target_link_libraries(beng-editor-core PUBLIC Comdlg32)

# Assimp + zlib (prebuilt .lib из thirdparty/assimplib) — нужны на
# этапе линковки exe: SkinnedMeshComponent/Material используют Assimp
# (паттерн gravelands-client/model_viewer)
target_link_directories(beng-editor-core PUBLIC ${assimp_lib})
target_link_libraries(beng-editor-core PUBLIC
    debug assimpd
    debug zlibstaticd
    optimized assimp
    optimized zlibstatic
)

# Тонкий exe единого эдитора
add_executable(beng-editor main/main.cpp)
target_link_libraries(beng-editor PRIVATE beng-editor-core)
set_target_properties(beng-editor PROPERTIES FOLDER "beng")

# UI-шрифты (иконки/моно) — рядом с exe (см. CopyUiFonts в корневом CMake)
CopyUiFonts(beng-editor)
