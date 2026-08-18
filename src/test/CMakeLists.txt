set(BIN blib_test)

file(GLOB test_sources "src/impl/*.cpp")

add_executable(${BIN} ${test_sources})

target_include_directories(${BIN} PRIVATE ${root_dir})

target_compile_definitions(${BIN} PRIVATE TEST_SOURCE_DIR="${root_dir}")

target_link_libraries(${BIN} PUBLIC blib)

add_test(NAME ${BIN} COMMAND ${BIN})
