#beng — Bulat Engine: ECS-ядро движка (Scene, Component, System, Transform)
#Зависит только от blib-core (blib-system транзитивно).
#Графика и 3D-вьювер вынесены в misc/model_viewer.

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

# Пока beng собирается только как static library
add_library(beng STATIC
    config.h

    ${core_header_src}
    ${core_impl_src}

    ${component_header_src}
    ${component_impl_src}

    ${system_header_src}
    ${system_impl_src}
)

set_target_properties(beng PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/...>, корень — src/
target_include_directories(beng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# blib-core передаёт blib-system и все конфигурационные дефайны (PUBLIC)
target_link_libraries(beng PUBLIC blib-core)
