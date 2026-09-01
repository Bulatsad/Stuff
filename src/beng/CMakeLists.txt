#beng

set (BIN beng)

project(${BIN})

file (GLOB global_header_src "CmakeLists.txt" "*.h")

file (GLOB main_header_src "*main/*.h")
file (GLOB main_impl_src   "*main/impl/*.cpp")

file (GLOB graphics_header_src "*graphics/*.h")
file (GLOB graphics_impl_src   "*graphics/impl/*.cpp")

GroupSoureces("" "${global_header_src}") 

GroupSoureces("${HeaderGroupPrefix}/main" "${main_header_src}")
GroupSoureces("${SourceGroupPrefix}/main" "${main_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/graphics" "${graphics_header_src}")
GroupSoureces("${SourceGroupPrefix}/graphics" "${graphics_impl_src}")  

set(beng_sources 
    ${global_header_src}

    ${main_header_src}
    ${main_impl_src} 

	${graphics_header_src}
    ${graphics_impl_src} 

)

message ( ${global_header_src})

add_executable(${BIN} ${beng_sources})

target_link_directories(${BIN} PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
target_link_directories(${BIN} PRIVATE ${assimp_lib})

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

set(debug_library_list debug  )

target_link_libraries(${BIN} 
	PUBLIC debug blib-graphics 
	PUBLIC debug assimpd 
	PUBLIC debug zlibstaticd 
	PUBLIC optimized blib-graphics 
	PUBLIC optimized assimp  
	PUBLIC optimized zlibstatic
)

target_include_directories(${BIN} PRIVATE ${blib_dir})
target_include_directories(${BIN} PRIVATE ${assimp_include})
target_include_directories(${BIN} PRIVATE ${stb_include})
target_include_directories(${BIN} PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/../")

if(WIN32)
    #target_link_libraries(${BIN} PUBLIC Glu32)

	#set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT)
	set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}" LINKER_LANGUAGE CXX)
	set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )
	
	message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
endif()
