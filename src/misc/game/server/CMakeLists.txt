#server

set(HeaderGroupPrefix "Header Files")
set(SourceGroupPrefix "Source Files")
function(GroupSoureces Prefix_ sources_)
    foreach(FILE ${sources_})
        source_group("${Prefix_}" FILES "${FILE}")
    endforeach()
endfunction()

set (BIN server)

file (GLOB engine_header "*engine/main/*.h")
file (GLOB engine_impl "*engine/main/*.cpp")

file (GLOB bridge_header "*engine/bridge/*.h")
file (GLOB bridge_impl "*engine/bridge/*.cpp")

file (GLOB components_header "*engine/components/*.h")
file (GLOB components_impl "*engine/components/*.cpp")

file (GLOB events_header "*engine/events/*.h")
file (GLOB events_impl "*engine/events/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/main" "${engine_header}")
GroupSoureces("${SourceGroupPrefix}/main" "${engine_impl}")

GroupSoureces("${HeaderGroupPrefix}/bridge" "${bridge_header}")
GroupSoureces("${SourceGroupPrefix}/bridge" "${bridge_impl}")

GroupSoureces("${HeaderGroupPrefix}/components" "${components_header}")
GroupSoureces("${SourceGroupPrefix}/components" "${components_impl}")

GroupSoureces("${HeaderGroupPrefix}/events" "${events_header}")
GroupSoureces("${SourceGroupPrefix}/events" "${events_impl}")


add_executable(${BIN} 

${engine_header}
${engine_impl}

${bridge_header}
${bridge_impl}

${components_header}
${components_impl}

${events_header}
${events_impl}

)


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

target_link_libraries(${BIN} PUBLIC blib-core)

target_include_directories(${BIN} PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/")

if(WIN32)
	#set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT)
	set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}" LINKER_LANGUAGE CXX)
	set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )
	
	message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
endif()