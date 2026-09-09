# gravelands-server — серверная часть игры Gravelands
#
# Паттерн «lib + тонкий exe»:
#   gravelands-server-core — вся логика сервера (ServerCore, frame-API)
#   gravelands-server      — тонкая обёртка: только main() и главный цикл

file (GLOB core_header_src "core/*.h")
file (GLOB core_impl_src "core/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/core" "${core_header_src}")
GroupSoureces("${SourceGroupPrefix}/core" "${core_impl_src}")

# Core-библиотека сервера: ECS-симуляция на beng-core
add_library(gravelands-server-core STATIC
    ${core_header_src}
    ${core_impl_src}
)
set_target_properties(gravelands-server-core PROPERTIES FOLDER "gravelands/server" LINKER_LANGUAGE CXX)

# gravelands-common (INTERFACE) даёт <gravelands/...> и blib-core;
# beng-core приходит транзитивно через TransformComponent и т.п. — линкуем явно
target_link_libraries(gravelands-server-core PUBLIC gravelands-common beng-core)

# Тонкий exe: владеет главным циклом
add_executable(gravelands-server main/main.cpp)
set_target_properties(gravelands-server PROPERTIES FOLDER "gravelands/server")
target_link_libraries(gravelands-server PRIVATE gravelands-server-core)
