#model_viewer — 3D-вьювер моделей (Assimp + Dear ImGui) поверх blib-graphics.
#Собирается только под Windows (добавляется из корневого CMake при
#наличии таргета blib-graphics).
#
#Легаси-исходники graphics/ (beng::graphics::Model/GameObject/Scene)
#остались в дереве на будущее, но в сборку НЕ включены:
#это стабы со ссылками на старый API (<beng/graphics/...> в старом
#расположении) — текущий main.cpp их не использует (работает через
#blib::graphics::SkinModel).

file (GLOB main_header_src "main/*.h")
file (GLOB main_impl_src "main/impl/*.cpp")

# Старый снапшот главного цикла — не собирать
list (FILTER main_impl_src EXCLUDE REGEX "main_OLD")

# Легаси-заголовки graphics/ — только для видимости в Solution Explorer
file (GLOB graphics_header_src "graphics/*.h")

GroupSoureces("${HeaderGroupPrefix}/main" "${main_header_src}")
GroupSoureces("${SourceGroupPrefix}/main" "${main_impl_src}")
GroupSoureces("${HeaderGroupPrefix}/graphics/legacy" "${graphics_header_src}")

add_executable(model_viewer
    ${main_header_src}
    ${main_impl_src}

    ${graphics_header_src}
)

set_target_properties(model_viewer PROPERTIES FOLDER "misc")

# blib-graphics (PUBLIC) даёт: корень инклюдов src/, конфигурационные
# дефайны, Assimp-хедеры, ImGui-хедеры, OpenGL-хедеры
target_link_libraries(model_viewer PRIVATE blib-graphics)

# Assimp + zlib (prebuilt .lib из thirdparty/assimplib)
target_link_directories(model_viewer PRIVATE ${assimp_lib})
target_link_libraries(model_viewer PRIVATE
    debug assimpd
    debug zlibstaticd
    optimized assimp
    optimized zlibstatic
)
