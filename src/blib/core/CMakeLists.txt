#blib-core — переносимое ядро: потоки/строки, algorithm (FFT, hash, сжатие),
#math, pdl. Зависит только от blib-system.

file (GLOB core_header_src "*.h")
file (GLOB core_impl_src "*.cpp")

file (GLOB algorithm_header_src "algorithm/*.h")
file (GLOB algorithm_impl_src "algorithm/impl/*.cpp")

file (GLOB compression_header_src "algorithm/compression/*.h")
file (GLOB compression_impl_src "algorithm/compression/impl/*.cpp")

file (GLOB hash_header_src "algorithm/hash/*.h")
file (GLOB hash_impl_src "algorithm/hash/impl/*.cpp")

file (GLOB math_header_src "math/*.h")
file (GLOB math_impl_src "math/impl/*.inl" "math/impl/*.cpp")

file (GLOB pdl_header_src "pdl/*.h")
file (GLOB pdl_impl_src "pdl/impl/*.cpp")

GroupSoureces("${HeaderGroupPrefix}/core" "${core_header_src}")
GroupSoureces("${SourceGroupPrefix}/core" "${core_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/core/algorithm" "${algorithm_header_src}")
GroupSoureces("${SourceGroupPrefix}/core/algorithm" "${algorithm_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/core/algorithm/compression" "${compression_header_src}")
GroupSoureces("${SourceGroupPrefix}/core/algorithm/compression" "${compression_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/core/algorithm/hash" "${hash_header_src}")
GroupSoureces("${SourceGroupPrefix}/core/algorithm/hash" "${hash_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/core/math" "${math_header_src}")
GroupSoureces("${SourceGroupPrefix}/core/math" "${math_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/core/pdl" "${pdl_header_src}")
GroupSoureces("${SourceGroupPrefix}/core/pdl" "${pdl_impl_src}")

blib_add_module(blib-core
    ${core_header_src}
    ${core_impl_src}

    ${algorithm_header_src}
    ${algorithm_impl_src}

    ${compression_header_src}
    ${compression_impl_src}

    ${hash_header_src}
    ${hash_impl_src}

    ${math_header_src}
    ${math_impl_src}

    ${pdl_header_src}
    ${pdl_impl_src}
)

blib_configure_module(blib-core blib_core_export)

target_link_libraries(blib-core PUBLIC blib-system)
