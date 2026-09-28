# gravelands-world — мир Gravelands: общая ECS-сцена игры (контент +
# системы). Мир НЕ владеет окном/камерой/вводом/ImGui — их даёт хост.
# Используется клиентом (gravelands-client-core) и эдитором
# (beng-editor + плагин игры, см. ARCHITECTURE.md): оба правят одну
# и ту же сцену через World.

file (GLOB world_header_src "*.h")
file (GLOB world_impl_src "*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/world" "${world_header_src}")
GroupSoureces("${SourceGroupPrefix}/world" "${world_impl_src}")

add_library(gravelands-world STATIC
    ${world_header_src}
    ${world_impl_src}
)

set_target_properties(gravelands-world PROPERTIES FOLDER "gravelands/world" LINKER_LANGUAGE CXX)

# Инклюд-корень game-слоя: src/misc — все инклюды игры имеют вид <gravelands/...>
target_include_directories(gravelands-world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# gravelands-common — <gravelands/...> и blib-core; beng-core — ECS;
# beng-client — рендер-ECS (компоненты/системы); blib-graphics — меши,
# материалы, примитивы, клавиатура (дебаг-клавиши света)
target_link_libraries(gravelands-world PUBLIC gravelands-common beng-core beng-client blib-graphics)
