#sc2img — конвертер текстур Stronghold (TGX/GM1 → PNG/BMP/JPG):
#GUI (вкладка изображений с превью) + headless CLI.
#Паттерн «lib + тонкий exe»:
#  - sc2img-core: документ (загрузка .tgx/.gm1) + экспорт изображений,
#    без GUI-зависимостей (только blib-graphics — там живут парсеры);
#  - sc2img-app: GUI-хост поверх каркаса beng::editor::EditorApplication
#    (центральная вкладка «Images», верхняя панель опций);
#  - sc2img: тонкий exe — main() разводит GUI/headless ветки.
#Собирается только под Windows (добавляется из корневого CMake при
#наличии таргета blib-graphics).

file (GLOB core_header_src "core/*.h")
file (GLOB core_impl_src "core/*.cpp")
file (GLOB gui_header_src "gui/*.h")
file (GLOB gui_impl_src "gui/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/sc2img/core" "${core_header_src}")
GroupSoureces("${SourceGroupPrefix}/sc2img/core" "${core_impl_src}")
GroupSoureces("${HeaderGroupPrefix}/sc2img/gui" "${gui_header_src}")
GroupSoureces("${SourceGroupPrefix}/sc2img/gui" "${gui_impl_src}")
GroupSoureces("${SourceGroupPrefix}/sc2img/main" "main/main.cpp")

# Core-библиотека: загрузка/парсинг/экспорт (без GUI)
add_library(sc2img-core STATIC
    ${core_header_src}
    ${core_impl_src}
)
set_target_properties(sc2img-core PROPERTIES FOLDER "misc/sc2img" LINKER_LANGUAGE CXX)

# Корень инклюдов: <sc2img/...> разрешается от src/misc
target_include_directories(sc2img-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# blib-graphics: parsers::TGXFile/GM1File и blib::graphics::Image
target_link_libraries(sc2img-core PUBLIC blib-graphics)

# stb_image_write (вендорный single-header) — нужен только .cpp ядра
target_include_directories(sc2img-core PRIVATE ${stb_include})

# GUI-библиотека: хост EditorApplication + центральная вкладка
add_library(sc2img-app STATIC
    ${gui_header_src}
    ${gui_impl_src}
)
set_target_properties(sc2img-app PROPERTIES FOLDER "misc/sc2img" LINKER_LANGUAGE CXX)

target_include_directories(sc2img-app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# beng-editor-core тянет beng-client, beng-core и blib-graphics (PUBLIC):
# ImGui-хедеры, консоль, каркас — всё доступно транзитивно
target_link_libraries(sc2img-app PUBLIC sc2img-core beng-editor-core)

# Assimp + zlib (prebuilt .lib): нужны на этапе линковки exe
# (паттерн model_viewer — линкуют конечные exe)
target_link_directories(sc2img-app PUBLIC ${assimp_lib})
target_link_libraries(sc2img-app PUBLIC
    debug assimpd
    debug zlibstaticd
    optimized assimp
    optimized zlibstatic
)

# Тонкий exe: GUI по умолчанию, --headless — CLI-конвертация
add_executable(sc2img main/main.cpp)
target_link_libraries(sc2img PRIVATE sc2img-app)
set_target_properties(sc2img PROPERTIES FOLDER "misc/sc2img")

# UI-шрифты (иконки/моно) — рядом с exe (см. CopyUiFonts в корневом CMake)
CopyUiFonts(sc2img)
