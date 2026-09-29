# gravelands-common — общие определения игры Gravelands:
# константы конфигурации, бинарный сетевой протокол (protocol.h),
# общие компоненты ECS (UnitComponent — первый игровой компонент).
# Линкуется клиентом, сервером и плагином эдитора.

file (GLOB common_header_src "*.h")
file (GLOB common_impl_src "*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/common" "${common_header_src}")
GroupSoureces("${SourceGroupPrefix}/common" "${common_impl_src}")

add_library(gravelands-common STATIC
    ${common_header_src}
    ${common_impl_src}
)

set_target_properties(gravelands-common PROPERTIES FOLDER "gravelands/common" LINKER_LANGUAGE CXX)

# Инклюд-корень game-слоя: src/misc — все инклюды игры имеют вид <gravelands/...>
target_include_directories(gravelands-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# blib-core — корень <blib/...> и конфигурационные дефайны (PUBLIC);
# beng-core — компоненты (UnitComponent наследует beng::IComponent)
target_link_libraries(gravelands-common PUBLIC blib-core beng-core)
