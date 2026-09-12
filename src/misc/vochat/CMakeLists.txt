#vochat
set (BIN ${vochat_project_name})

file (GLOB_RECURSE vochat_sources "*src/*.h" "*src/*.cpp")

add_executable(${BIN} ${vochat_sources})

target_include_directories(${BIN} PRIVATE ${root_dir})
target_include_directories(${BIN} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${BIN} 
	PUBLIC blib-core 
	PUBLIC blib-sound 
	PUBLIC blib-network
)

