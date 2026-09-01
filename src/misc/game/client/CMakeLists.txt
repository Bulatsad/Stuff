#client
set (BIN client)

file (GLOB_RECURSE src "*.h" "*.cpp")
file (GLOB bridge_header "../server/engine/bridge/*.h")

add_executable(${BIN} ${src} ${bridge_header})

target_link_directories(${BIN} PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set(blib_link_type_enum blib_link_static blib_link_dynamic)
set(blib_link_type STATIC CACHE STRING "Library link type")
set_property(CACHE blib_link_type PROPERTY STRINGS ${blib_link_type_enum})

target_compile_definitions(${BIN} PRIVATE blib_import)
if(blib_link_type STREQUAL "blib_link_static")
	target_compile_definitions(${BIN} PRIVATE blib_static)
elseif(blib_link_type STREQUAL "blib_link_dynamic")
	target_compile_definitions(${BIN} PRIVATE blib_dynamic)
else()
    message(FATAL_ERROR "Undefined blib link type")
endif()

target_link_libraries(${BIN} PUBLIC blib-graphics)

if(WIN32)
	#set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT)
	set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}" LINKER_LANGUAGE CXX)
	set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )
	
	message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
endif()