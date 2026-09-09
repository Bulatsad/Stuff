# gravelands-client — клиентская часть игры Gravelands
#
# Паттерн «lib + тонкий exe»:
#   gravelands-client-core — вся логика клиента (ClientCore, frame-API)
#   gravelands-client      — тонкая обёртка: только main() и главный цикл

file (GLOB core_header_src "core/*.h")
file (GLOB core_impl_src "core/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/core" "${core_header_src}")
GroupSoureces("${SourceGroupPrefix}/core" "${core_impl_src}")

# Core-библиотека клиента. Пока рендерит напрямую через blib-graphics;
# рендер-ECS (beng-client) придёт позже — см. ARCHITECTURE.md
add_library(gravelands-client-core STATIC
    ${core_header_src}
    ${core_impl_src}
)
set_target_properties(gravelands-client-core PROPERTIES FOLDER "gravelands/client" LINKER_LANGUAGE CXX)

# gravelands-common даёт <gravelands/...> и blib-core;
# beng-core — Time; blib-graphics — окно/рендер/камера
target_link_libraries(gravelands-client-core PUBLIC gravelands-common beng-core blib-graphics)

# Assimp + zlib (prebuilt .lib из thirdparty/assimplib) — нужны blib-graphics
# (material.obj) при использовании Mesh/Material
target_link_directories(gravelands-client-core PUBLIC ${assimp_lib})
target_link_libraries(gravelands-client-core PUBLIC
    debug assimpd
    debug zlibstaticd
    optimized assimp
    optimized zlibstatic
)

# Тонкий exe: владеет главным циклом
add_executable(gravelands-client main/main.cpp)
set_target_properties(gravelands-client PROPERTIES FOLDER "gravelands/client")
target_link_libraries(gravelands-client PRIVATE gravelands-client-core)
