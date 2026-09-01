#blib-sound — запись/воспроизведение звука (WinMM)
#(реализация есть только под Windows)

file (GLOB sound_header_src "*.h")
file (GLOB sound_impl_src "impl/*.cpp")

file (GLOB sound_platform_impl_header "impl/win/*.h")
file (GLOB sound_platform_impl_src "impl/win/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/sound" "${sound_header_src}")
GroupSoureces("${HeaderGroupPrefix}/sound" "${sound_platform_impl_header}")
GroupSoureces("${SourceGroupPrefix}/sound" "${sound_impl_src}")
GroupSoureces("${SourceGroupPrefix}/sound" "${sound_platform_impl_src}")

blib_add_module(blib-sound
    ${sound_header_src}
    ${sound_platform_impl_header}
    ${sound_impl_src}
    ${sound_platform_impl_src}
)

blib_configure_module(blib-sound blib_sound_export)

target_link_libraries(blib-sound PUBLIC blib-core)
target_link_libraries(blib-sound PRIVATE Winmm)
