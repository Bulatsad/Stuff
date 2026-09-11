#beng-client — клиентская среда движка: рендер-ECS (компоненты
#скелетных моделей и анимаций + системы анимации и отрисовки).
#Зависит от beng-core и blib-graphics; добавляется из корневого
#CMake только при наличии таргета blib-graphics (сейчас — Windows).

file (GLOB component_header_src "components/*.h")
file (GLOB component_impl_src "components/*.cpp")

file (GLOB system_header_src "systems/*.h")
file (GLOB system_impl_src "systems/*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/client/components" "${component_header_src}")
GroupSoureces("${SourceGroupPrefix}/client/components" "${component_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/client/systems" "${system_header_src}")
GroupSoureces("${SourceGroupPrefix}/client/systems" "${system_impl_src}")

add_library(beng-client STATIC
    ${component_header_src}
    ${component_impl_src}

    ${system_header_src}
    ${system_impl_src}
)

set_target_properties(beng-client PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/client/...>, корень — src/beng
target_include_directories(beng-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# beng-core даёт ECS-ядро, blib-graphics — SkinModel/Animator/RenderTarget
target_link_libraries(beng-client PUBLIC beng-core blib-graphics)
