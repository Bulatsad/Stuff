#beng-editor — каркас эдитора: панели на Dear ImGui (Hierarchy,
#анимации, вьюпорт, опции рендера, консоль). Панели реализуют
#IPanel и не зависят от конкретного приложения — их можно
#переиспользовать в любом инструменте на базе beng.
#Добавляется из корневого CMake при наличии blib-graphics.

file (GLOB panel_header_src "panels/*.h")
file (GLOB panel_impl_src "panels/*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/editor/panels" "${panel_header_src}")
GroupSoureces("${SourceGroupPrefix}/editor/panels" "${panel_impl_src}")

add_library(beng-editor STATIC
    ${panel_header_src}
    ${panel_impl_src}
)

set_target_properties(beng-editor PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/editor/...>, корень — src/beng
target_include_directories(beng-editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# beng-client даёт компоненты (AnimatorComponent в панели анимаций),
# blib-graphics — ImGui-хедеры, ConsoleWindow, OrbitCamera
target_link_libraries(beng-editor PUBLIC beng-client)
