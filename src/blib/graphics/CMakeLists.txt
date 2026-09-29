#blib-graphics — рендер (OpenGL/WGL, Win32-окно), ассеты, Dear ImGui
#(реализация есть только под Windows)

file (GLOB graphics_header_src "*.h")
file (GLOB graphics_impl_src "impl/*.cpp")

file (GLOB graphics_platform_impl_header "impl/win/*.h")
file (GLOB graphics_platform_impl_src "impl/win/*.cpp")

file (GLOB graphics_console_header_src "console/*.h")
file (GLOB graphics_console_impl_src "console/impl/*.cpp")

#dear imgui
file (GLOB imgui_sources_src "${imgui_dir}/imgui/*.h" "${imgui_dir}/imgui/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/graphics" "${graphics_header_src}")
GroupSoureces("${HeaderGroupPrefix}/graphics" "${graphics_platform_impl_header}")
GroupSoureces("${SourceGroupPrefix}/graphics" "${graphics_impl_src}")
GroupSoureces("${SourceGroupPrefix}/graphics" "${graphics_platform_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/graphics/console" "${graphics_console_header_src}")
GroupSoureces("${SourceGroupPrefix}/graphics/console" "${graphics_console_impl_src}")

#dear imgui
GroupSoureces("imgui" "${imgui_sources_src}")

blib_add_module(blib-graphics
    ${graphics_header_src}
    ${graphics_platform_impl_header}
    ${graphics_impl_src}
    ${graphics_platform_impl_src}

    ${graphics_console_header_src}
    ${graphics_console_impl_src}

    ${imgui_sources_src}
)

blib_configure_module(blib-graphics blib_graphics_export)

target_link_libraries(blib-graphics PUBLIC blib-core)
target_link_libraries(blib-graphics PUBLIC Opengl32)

# Assimp + zlib (prebuilt .lib из thirdparty/assimplib): material.obj и
# загрузчики моделей используют Assimp. В shared-сборке blib-graphics.dll
# обязана прилинковать их сама; в static — линкуют конечные exe (PUBLIC,
# паттерн gravelands/model_viewer)
target_link_directories(blib-graphics PUBLIC ${assimp_lib})
target_link_libraries(blib-graphics PUBLIC
    debug assimpd
    debug zlibstaticd
    optimized assimp
    optimized zlibstatic
)

target_include_directories(blib-graphics PUBLIC ${openglapi})
target_include_directories(blib-graphics PUBLIC ${assimp_include})
target_include_directories(blib-graphics PRIVATE ${stb_include})

#dear imgui
target_include_directories(blib-graphics PUBLIC ${imgui_dir})

# dear imgui: 32-битный ImWchar — КОСТЫЛЬ (см. BENG.md «Шрифты и иконки»):
# иконочный шрифт Material Design Icons использует коды за пределами BMP
# (0xF0000+), 16-битный ImWchar их не адресует. Правка вендорного
# imconfig.h запрещена правилами проекта — включаем макрос из CMake.
# imgui.cpp компилируется только в blib-graphics; PUBLIC расходится всем
# потребителям — ABI ImWchar обязан совпадать во всех TU процесса
target_compile_definitions(blib-graphics PUBLIC IMGUI_USE_WCHAR32)

if(COMPILE_ASSIMP_COMPATIBLE)
    target_compile_definitions(blib-graphics PUBLIC COMPILE_ASSIMP_COMPATIBLE)
endif()
