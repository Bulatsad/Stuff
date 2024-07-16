#blib

#function(filter s)
# foreach(FILE ${s})
#   get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
#   get_filename_component(FEXT "${FILE}" EXT)
#   get_filename_component(FNAME "${FILE}" NAME)
#   string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
#   if ("${FEXT}" MATCHES "cpp")
#     set(GROUP "Source Files${GROUP}")
#   elseif ("${FEXT}" MATCHES "h")
#     set(GROUP "Header Files${GROUP}")
#   endif()
#   #message(STATUS ${GROUP})
#   source_group(${GROUP} FILES "${FILE}")
# endforeach()
#endfunction()
#filter("${src}")

set(HeaderGroupPrefix "Header Files")
set(SourceGroupPrefix "Source Files")
function(GroupSoureces Prefix_ sources_)
    foreach(FILE ${sources_})
        source_group("${Prefix_}" FILES "${FILE}")
    endforeach()
endfunction()

set (BIN blib)

project(${BIN})


file (GLOB blibGloabal "CmakeLists.txt" "*.h")

file (GLOB sound_header_src "*sound/*.h")

file (GLOB thread_header_src "*thread/*.h")

file (GLOB core_header_src "*core/*.h")
file (GLOB core_impl_src "*core/*.cpp")

file (GLOB algorithm_header_src "*algorithm/*.h")
file (GLOB algorithm_impl_src "*algorithm/impl/*.cpp")

file (GLOB network_header_src "*network/*.h")
file (GLOB network_impl_src "*network/impl/*.cpp")

file (GLOB graphics_header_src "*graphics/*.h")
#file (GLOB graphics_impl_src "*graphics/impl/*.cpp")


if(WIN32)
    file (GLOB sound_platform_impl_src "*sound/impl/win/*.cpp" "*sound/impl/*.cpp" "*sound/impl/win/*.h")
    file (GLOB sound_platform_impl_header "*sound/impl/win/*.h")

    file (GLOB thread_platform_impl_src "*thread/impl/win/*.cpp")
    
    file (GLOB network_platform_impl_src "*network/impl/win/*.cpp")
    file (GLOB network_platform_impl_header "*network/impl/win/*.h")

    file (GLOB graphics_platform_impl_src "*graphics/impl/win/*.cpp")
    #file (GLOB network_platform_impl_header "*graphics/impl/win/*.h")
else()
endif()

GroupSoureces("" "${blibGloabal}")   

GroupSoureces("${HeaderGroupPrefix}/sound" "${sound_header_src}")
GroupSoureces("${HeaderGroupPrefix}/sound" "${sound_platform_impl_header}")
GroupSoureces("${SourceGroupPrefix}/sound" "${sound_platform_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/thread" "${thread_header_src}")
GroupSoureces("${SourceGroupPrefix}/thread" "${thread_platform_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/core" "${core_header_src}")  
GroupSoureces("${SourceGroupPrefix}/core" "${core_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/algorithm" "${algorithm_header_src}")  
GroupSoureces("${SourceGroupPrefix}/algorithm" "${algorithm_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/network" "${network_header_src}")  
GroupSoureces("${HeaderGroupPrefix}/network" "${network_platform_impl_header}")  
GroupSoureces("${SourceGroupPrefix}/network" "${network_impl_src}")  
GroupSoureces("${SourceGroupPrefix}/network" "${network_platform_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/graphics" "${graphics_header_src}")  
GroupSoureces("${HeaderGroupPrefix}/graphics" "${graphics_platform_impl_header}")  
GroupSoureces("${SourceGroupPrefix}/graphics" "${graphics_platform_impl_src}")  


add_library(${BIN} STATIC 
    ${blibGloabal} 

    ${sound_header_src}
    ${sound_platform_impl_header}
    ${sound_platform_impl_src} 

    ${thread_header_src}
    ${thread_platform_impl_src} 

    ${core_header_src} 
    ${core_impl_src}

    ${algorithm_header_src}
    ${algorithm_impl_src}

    ${network_header_src}
    ${network_impl_src}
    ${network_platform_impl_src}
    ${network_platform_impl_header}

    ${graphics_header_src}
    ${graphics_platform_impl_header}
    ${graphics_platform_impl_src}
)

target_include_directories(${BIN} PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/../")
#target_include_directories(${BIN} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/sound")
#target_include_directories(${BIN} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/thread")
#target_include_directories(${BIN} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/core")

set_target_properties(${BIN} PROPERTIES LINKER_LANGUAGE CXX)

if(WIN32)
	target_link_libraries(${BIN} PRIVATE Winmm)
	target_link_libraries(${BIN} PRIVATE Ws2_32)
    target_link_libraries(${BIN} PUBLIC Opengl32)

	set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}" LINKER_LANGUAGE CXX)
    set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )
	
	message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
else()
endif()

