#blib-system — нижний уровень: платформенные примитивы
#(thread, memory) и базовые заголовки blib (align, blibint, config, inline, utilmacro)

file (GLOB thread_header_src "thread/*.h")

file (GLOB memory_header_src "memory/*.h" "memory/allocators/*.h")
file (GLOB memory_impl_src "memory/impl/*.cpp" "memory/impl/*.inl")

# Базовые заголовки лежат в корне blib/ (общие для всех модулей)
set(blib_base_headers
    "${CMAKE_CURRENT_SOURCE_DIR}/../align.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../blibint.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../config.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../inline.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../utilmacro.h"
)

if(WIN32)
    file (GLOB thread_platform_impl_src "thread/impl/win/*.cpp")
else()
    file (GLOB thread_platform_impl_src "thread/impl/linux/*.cpp")
endif()

GroupSoureces("${HeaderGroupPrefix}/system/thread" "${thread_header_src}")
GroupSoureces("${SourceGroupPrefix}/system/thread" "${thread_platform_impl_src}")

GroupSoureces("${HeaderGroupPrefix}/system/memory" "${memory_header_src}")
GroupSoureces("${SourceGroupPrefix}/system/memory" "${memory_impl_src}")

blib_add_module(blib-system
    ${blib_base_headers}

    ${thread_header_src}
    ${thread_platform_impl_src}

    ${memory_header_src}
    ${memory_impl_src}
)

blib_configure_module(blib-system blib_system_export)

# pthread нужен для rwlock на линуксе
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(blib-system PUBLIC Threads::Threads)
endif()
