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

add_subdirectory(${blib_dir})
add_subdirectory(${beng_dir})

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(${root_dir}/test)
endif()
