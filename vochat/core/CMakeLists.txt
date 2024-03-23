#core
set (BIN core)

file (GLOB_RECURSE src "*src/*.h" "*src/*.cpp")

add_executable(${BIN} ${src})

target_include_directories(${BIN} PRIVATE ${blib_dir}/sound)

target_link_directories(${BIN} PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

target_link_libraries(${BIN} PUBLIC blib)

if(WIN32)
	#set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT)
	set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}" LINKER_LANGUAGE CXX)
	set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )
	
	message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
endif()