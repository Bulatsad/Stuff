#sndChat
cmake_minimum_required(VERSION 3.18)

project(vochat)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(blib_dir ${CMAKE_CURRENT_SOURCE_DIR}/blib)

add_subdirectory(blib)
add_subdirectory(core)