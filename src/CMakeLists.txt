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

option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
	enable_testing()
endif()

add_subdirectory(${blib_dir})

# beng-core — ECS-ядро движка (Bulat Engine); от графики не зависит
add_subdirectory(${beng_dir})

# beng-client — рендер-ECS движка; требует blib-graphics (сейчас — Windows)
if(TARGET blib-graphics)
	add_subdirectory(${beng_dir}/client)

	# beng-editor — панели эдитора на ImGui; тоже требует blib-graphics
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

# Тесты движка (BUILD_TESTS) — по образцу blib/test
if(BUILD_TESTS)
	add_subdirectory(${beng_dir}/test)
endif()
