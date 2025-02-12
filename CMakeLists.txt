cmake_minimum_required(VERSION 3.29)

project(Stuff)

set(HeaderGroupPrefix "Header Files")
set(SourceGroupPrefix "Source Files")
function(GroupSoureces Prefix_ sources_)
    foreach(FILE ${sources_})
        source_group("${Prefix_}" FILES "${FILE}")
    endforeach()
endfunction()

set(blib_dir   ${CMAKE_CURRENT_SOURCE_DIR}/blib)

set(thirdparty_dir ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

set(assimp_dir ${thirdparty_dir}/assimplib)
set(assimp_lib ${assimp_dir}/lib)
set(assimp_include ${assimp_dir}/include)

set(stb_dir ${thirdparty_dir}/stb)
set(stb_include ${stb_dir})

add_subdirectory(blib)
add_subdirectory(beng)