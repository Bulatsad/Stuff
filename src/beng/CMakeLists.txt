#beng-core — Bulat Engine: переносимое ядро движка (ECS: Scene, Component,
#System, Transform; Time). Ни графики, ни сети, ни звука.
#Зависит только от blib-core (blib-system транзитивно).
#Остальные таргеты движка (beng-client/beng-server/beng-editor) появятся позже.

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

# Пока beng-core собирается только как static library
add_library(beng-core STATIC
    config.h

    ${core_header_src}
    ${core_impl_src}

    ${component_header_src}
    ${component_impl_src}

    ${system_header_src}
    ${system_impl_src}
)

set_target_properties(beng-core PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/...>, корень — src/
target_include_directories(beng-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# blib-core передаёт blib-system и все конфигурационные дефайны (PUBLIC)
target_link_libraries(beng-core PUBLIC blib-core)
