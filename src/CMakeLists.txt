cmake_minimum_required(VERSION 3.29)

project(Stuff)

set(HeaderGroupPrefix "Header Files")
set(SourceGroupPrefix "Source Files")
function(GroupSoureces Prefix_ sources_)
    foreach(FILE ${sources_})
        source_group("${Prefix_}" FILES "${FILE}")
    endforeach()
endfunction()

set(blib_project_name blib)
set(beng_project_name beng)

set(root_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(blib_dir ${root_dir}/blib)
set(beng_dir ${root_dir}/beng)

set(thirdparty_dir ${root_dir}/thirdparty)

set(assimp_dir ${thirdparty_dir}/assimplib)
set(assimp_lib ${assimp_dir}/lib)
set(assimp_include ${assimp_dir}/include)

set(openglapi ${thirdparty_dir}/opengl)

set(imgui_dir ${thirdparty_dir}/imgui)

set(stb_dir ${thirdparty_dir}/stb)
set(stb_include ${stb_dir})

# UI-шрифты (иконки MDPI + моно JetBrains Mono): вендорные TTF + тексты
# лицензий. Копируются рядом с exe, которые их грузят (см. CopyUiFonts)
set(fonts_dir ${thirdparty_dir}/fonts)

# Копирование каталога UI-шрифтов к exe (движок ищет их относительно
# исполняемого файла — см. editorApplication.cpp, resolveEditorFontPath)
function(CopyUiFonts target_)
	add_custom_command(TARGET ${target_} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${fonts_dir}" "$<TARGET_FILE_DIR:${target_}>/fonts"
		COMMENT "Copying UI fonts next to ${target_}")
endfunction()

option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
	enable_testing()
endif()

# Типы сборки blib/beng объявляются и в их CMakeLists (CACHE — повторная
# установка не перезаписывает пользовательское значение). Здесь они нужны
# ДО подключения подкаталогов: CMAKE_RUNTIME_OUTPUT_DIRECTORY фиксируется
# в свойстве каждого таргета в момент его создания
set(blib_build_type blib_build_static CACHE STRING "blib library build type")
set(beng_build_type beng_build_static CACHE STRING "beng library build type")

# Shared-сборка (blib/beng — DLL): все exe и DLL кладутся в общий
# bin-каталог — Windows ищет DLL рядом с exe, а в дефолтной раскладке
# модулей (blib/system/Debug, beng/Debug, ...) exe их не нашли бы
if(blib_build_type STREQUAL "blib_build_dynamic")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

add_subdirectory(${blib_dir})

# beng-core — ECS-ядро движка (Bulat Engine); от графики не зависит
add_subdirectory(${beng_dir})

# beng-client — рендер-ECS движка; требует blib-graphics (сейчас — Windows)
if(TARGET blib-graphics)
	add_subdirectory(${beng_dir}/client)

	# beng-editor-core (каркас: EditorApplication + панели ImGui) +
	# beng-editor (единый exe эдитора); тоже требуют blib-graphics
	add_subdirectory(${beng_dir}/editor)
endif()

# beng/test_ecs — демо/Smoke-приложение ECS-ядра (Scene + Transform-иерархия)
add_subdirectory(${beng_dir}/test_ecs)

# misc/model_viewer — 3D-вьювер поверх blib-graphics; собирается только
# если модуль графики доступен (на текущий момент — только Windows)
if(TARGET blib-graphics)
	add_subdirectory(misc/model_viewer)
endif()

# misc/gravelands — игра Gravelands (диаблоид): библиотеки + тонкие exe
add_subdirectory(misc/gravelands)

# Единый эдитор с плагином игры (плагин-модель, см. ARCHITECTURE.md,
# «Эдитор»). Режим доставки — gravelands_plugin_type:
#   STATIC (этап 1): плагин линкуется в beng-editor.exe — удобная
#     отладка; exe зовёт фабрику напрямую (дефайн ..._STATIC);
#   SHARED (этап 2): gravelands.dll грузится эдитором через
#     LoadLibrary/GetProcAddress (дефайн ..._DLL) — exe игру не знает.
# Код плагина один и тот же — меняется только способ доставки.
if(TARGET beng-editor AND TARGET gravelands-plugin)
	if(gravelands_plugin_type STREQUAL "SHARED")
		target_compile_definitions(beng-editor PRIVATE BENG_EDITOR_GRAVELANDS_DLL)
	else()
		target_link_libraries(beng-editor PRIVATE gravelands-plugin)
		target_compile_definitions(beng-editor PRIVATE BENG_EDITOR_GRAVELANDS_STATIC)
	endif()
endif()

# Тесты движка (BUILD_TESTS) — по образцу blib/test
if(BUILD_TESTS)
	add_subdirectory(${beng_dir}/test)
endif()
