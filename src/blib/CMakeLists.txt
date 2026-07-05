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

set (BIN blib)

project(${BIN})

set(blib_build_type_enum blib_build_static blib_build_dynamic)
set(blib_build_type STATIC CACHE STRING "Library build type")
set_property(CACHE blib_build_type PROPERTY STRINGS ${blib_build_type_enum})

set(blib_inline_mode_enum __blib_inline_always __blib_inline_never __blib_inline_compiler)
set(blib_inline_mode STATIC CACHE STRING "Inline mode")
set_property(CACHE blib_inline_mode PROPERTY STRINGS ${blib_inline_mode_enum})

set(____blib_configuration_platform_undefined 0)
set(____blib_configuration_platform_windows 1)
set(____blib_configuration_platform_linux 2)
set(____blib_configuration_platform_macos 3)

set(____blib_configuration_library_type_undefined 0)
set(____blib_configuration_library_type_static 1)
set(____blib_configuration_library_type_shared 2)

option(COMPILE_ASSIMP_COMPATIBLE "Enable Assimp compatibility (loadFromAssimp, Assimp headers)" ON)

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

file (GLOB math_header_src "*math/*.h")
file (GLOB math_impl_src "*math/impl/*.inl" "*math/impl/*.cpp")

#dear imgui
file (GLOB imgui_sources_src "${imgui_dir}/imgui/*.h" "${imgui_dir}/imgui/*.cpp")

if(WIN32)
    file (GLOB sound_platform_impl_src "*sound/impl/win/*.cpp" "*sound/impl/*.cpp" "*sound/impl/win/*.h")
    file (GLOB sound_platform_impl_header "*sound/impl/win/*.h")

    file (GLOB thread_platform_impl_src "*thread/impl/win/*.cpp")
    
    file (GLOB network_platform_impl_src "*network/impl/win/*.cpp")
    file (GLOB network_platform_impl_header "*network/impl/win/*.h")

    file (GLOB graphics_platform_impl_header "*graphics/impl/win/*.h")
    file (GLOB graphics_platform_impl_src "*graphics/impl/win/*.cpp")
    file (GLOB graphics_impl_src "*graphics/impl/*.cpp")

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
GroupSoureces("${SourceGroupPrefix}/graphics" "${graphics_impl_src}")  

GroupSoureces("${HeaderGroupPrefix}/math" "${math_header_src}")
GroupSoureces("${SourceGroupPrefix}/math/impl" "${math_impl_src}")  

#dear imgui
GroupSoureces("imgui" "${imgui_sources_src}")  


set(blib_sources 
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
    ${graphics_impl_src}

    ${math_header_src} 
    ${math_impl_src}
    
#dear imgui
    ${imgui_sources_src}
)

if(blib_build_type STREQUAL "blib_build_static")
	add_library(${BIN} STATIC ${blib_sources})
    target_compile_definitions(${BIN} PRIVATE ____blib_configuration_library_type_value=${____blib_configuration_library_type_static})
elseif(blib_build_type STREQUAL "blib_build_dynamic")
	add_library(${BIN} SHARED ${blib_sources})
    target_compile_definitions(${BIN} PRIVATE ____blib_configuration_library_type_value=${____blib_configuration_library_type_shared})
else()
    message(FATAL_ERROR "Undefined build type")
    message(FATAL_ERROR "blib_build_type is not defined")
endif()

if(blib_inline_mode STREQUAL "__blib_inline_always")
    target_compile_definitions(${BIN} PRIVATE __blib_inline_always)
elseif(blib_inline_mode STREQUAL "__blib_inline_never")
    target_compile_definitions(${BIN} PRIVATE __blib_inline_never)
elseif(blib_inline_mode STREQUAL "__blib_inline_compiler")
    target_compile_definitions(${BIN} PRIVATE __blib_inline_compiler)
else()
    message(FATAL_ERROR "Undefined inline mode")
endif()

target_compile_definitions(${BIN} PRIVATE ____blib_configuration_platform_undefined=${____blib_configuration_platform_undefined})
target_compile_definitions(${BIN} PRIVATE ____blib_configuration_platform_windows=${____blib_configuration_platform_windows})
target_compile_definitions(${BIN} PRIVATE ____blib_configuration_platform_linux=${____blib_configuration_platform_linux})
target_compile_definitions(${BIN} PRIVATE ____blib_configuration_platform_macos=${____blib_configuration_platform_macos})

if(COMPILE_ASSIMP_COMPATIBLE)
    target_compile_definitions(${BIN} PUBLIC COMPILE_ASSIMP_COMPATIBLE)
endif()

target_compile_definitions(${BIN} PRIVATE ____blib_configuration_library_type_undefined=${____blib_configuration_library_type_undefined})
target_compile_definitions(${BIN} PRIVATE ____blib_configuration_library_type_shared=${____blib_configuration_library_type_shared})
target_compile_definitions(${BIN} PRIVATE ____blib_configuration_library_type_static=${____blib_configuration_library_type_static})

target_compile_definitions(${BIN} PRIVATE blib_export)

target_include_directories(${BIN} PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/../")

#TODO: delete
target_include_directories(${BIN} PUBLIC ${assimp_include})
target_include_directories(${BIN} PUBLIC ${openglapi})

target_include_directories(${BIN} PRIVATE ${stb_include})

#target_include_directories(${BIN} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/sound")
#target_include_directories(${BIN} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/thread")
#target_include_directories(${BIN} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/core")

#dear imgui
target_include_directories(${BIN} PUBLIC ${imgui_dir})

set_target_properties(${BIN} PROPERTIES LINKER_LANGUAGE CXX)

if(WIN32)
	target_link_libraries(${BIN} PRIVATE Winmm)
	target_link_libraries(${BIN} PRIVATE Ws2_32)
    target_link_libraries(${BIN} PUBLIC Opengl32)
    #target_link_libraries(${BIN} PUBLIC Glu32)

	set_target_properties(${BIN} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}" LINKER_LANGUAGE CXX)
    set (CMAKE_CXX_FLAGS_RELEASE "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_MINSIZEREL "/EHsc /Gd /Gy /Oi /Gm- /MP /O2 /W3 /ZI /MD" )
    set (CMAKE_CXX_FLAGS_DEBUG "/EHsc /Gd /Gy /GS /Oi /Gm- /MP /Od /W3 /ZI /MDd" )
	

    target_compile_definitions(${BIN} PRIVATE ____blib_configuration_platform_value=${____blib_configuration_platform_windows})

	message(STATUS ${CMAKE_CXX_FLAGS_DEBUG})
else()
endif()

