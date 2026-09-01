#blib-network — сокеты (UDP/TCP, winsock)
#(реализация есть только под Windows)

file (GLOB network_header_src "*.h")

file (GLOB network_platform_impl_header "impl/win/*.h")
file (GLOB network_platform_impl_src "impl/win/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/network" "${network_header_src}")
GroupSoureces("${HeaderGroupPrefix}/network" "${network_platform_impl_header}")
GroupSoureces("${SourceGroupPrefix}/network" "${network_platform_impl_src}")

blib_add_module(blib-network
    ${network_header_src}
    ${network_platform_impl_header}
    ${network_platform_impl_src}
)

blib_configure_module(blib-network blib_network_export)

target_link_libraries(blib-network PUBLIC blib-core)
target_link_libraries(blib-network PRIVATE Ws2_32)
