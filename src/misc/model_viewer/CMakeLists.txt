#model_viewer — 3D-вьювер моделей (прототип эдитора).
#Паттерн «lib + тонкий exe»:
#  - model_viewer-core: ViewerCore (frame-API initialize/tick/shutdown),
#    ECS-сцена beng, панели beng-editor, рендер в FBO вьюпорта;
#  - model_viewer: тонкий exe, только main().
#Собирается только под Windows (добавляется из корневого CMake при
#наличии таргета blib-graphics).

file (GLOB core_header_src "core/*.h")
file (GLOB core_impl_src "core/*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/model_viewer/core" "${core_header_src}")
GroupSoureces("${SourceGroupPrefix}/model_viewer/core" "${core_impl_src}")
GroupSoureces("${SourceGroupPrefix}/model_viewer/main" "main/main.cpp")

# Core-библиотека: вся логика вьювера
add_library(model_viewer-core STATIC
    ${core_header_src}
    ${core_impl_src}
)

set_target_properties(model_viewer-core PROPERTIES FOLDER "misc/model_viewer" LINKER_LANGUAGE CXX)

# Корень инклюдов: <model_viewer/...> разрешается от src/misc
# (имя каталога повторяет имя таргета — как <gravelands/...>)
target_include_directories(model_viewer-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# beng-editor тянет beng-client, beng-core и blib-graphics (PUBLIC):
# ImGui-хедеры, консоль, рендер — всё доступно транзитивно
target_link_libraries(model_viewer-core PUBLIC beng-editor)

# Assimp + zlib (prebuilt .lib из thirdparty/assimplib) — нужны
# на этапе линковки exe (Importer::ReadFile и зилевые символы)
target_link_directories(model_viewer-core PUBLIC ${assimp_lib})
target_link_libraries(model_viewer-core PUBLIC
    debug assimpd
    debug zlibstaticd
    optimized assimp
    optimized zlibstatic
)

# Тонкий exe
add_executable(model_viewer main/main.cpp)
target_link_libraries(model_viewer PRIVATE model_viewer-core)
set_target_properties(model_viewer PROPERTIES FOLDER "misc/model_viewer")
