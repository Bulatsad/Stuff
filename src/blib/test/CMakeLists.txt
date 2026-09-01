# ============================================================
# blib tests
#
# Each test group is compiled exactly once as an OBJECT library
# and then linked into its own executable and into the aggregate
# blib_test_all. OBJECT libraries are immune to MSVC linker
# dead-stripping (unlike static libs), so the BLIB_TEST_CASE
# self-registration works without /WHOLEARCHIVE.
# ============================================================

# Shared main.cpp (BLIB_TEST_MAIN) — compiled once
add_library(blib_test_main OBJECT src/impl/main.cpp)
target_include_directories(blib_test_main PRIVATE ${root_dir})
# usage-требования blib-core (инклюды + дефайны конфигурации) нужны и объектным библиотекам
target_link_libraries(blib_test_main PRIVATE blib-core)
set_target_properties(blib_test_main PROPERTIES FOLDER "blib/tests")

# Test group list: group name == file suffix test<group>.cpp
set(blib_test_groups angle matrix pdl quaternion vector allocator iterator stream binary compression hash)

# Список standalone-исполняемых групп (заполняется в цикле ниже)
set(blib_test_group_exes)

foreach(group ${blib_test_groups})
	# OBJECT library holding the group's tests
	set(group_obj blib_test_${group}_obj)
	add_library(${group_obj} OBJECT src/impl/test${group}.cpp)
	target_include_directories(${group_obj} PRIVATE ${root_dir})
	target_compile_definitions(${group_obj} PRIVATE TEST_SOURCE_DIR="${root_dir}")
	target_link_libraries(${group_obj} PRIVATE blib-core)
	set_target_properties(${group_obj} PROPERTIES FOLDER "blib/tests/${group}")

	# Standalone executable for this group: main + group objects
	set(group_exe blib_test_${group})
	add_executable(${group_exe})
	target_sources(${group_exe} PRIVATE
		$<TARGET_OBJECTS:blib_test_main>
		$<TARGET_OBJECTS:${group_obj}>
	)
	target_link_libraries(${group_exe} PRIVATE blib-core)
	set_target_properties(${group_exe} PROPERTIES FOLDER "blib/tests/${group}")
	add_test(NAME ${group_exe} COMMAND ${group_exe})
	list(APPEND blib_test_group_exes ${group_exe})
endforeach()

# Aggregate executable: runs every group in one process
# (convenience only — a crash in one group kills the whole run)
add_executable(blib_test_all)
target_sources(blib_test_all PRIVATE $<TARGET_OBJECTS:blib_test_main>)
foreach(group ${blib_test_groups})
	target_sources(blib_test_all PRIVATE $<TARGET_OBJECTS:blib_test_${group}_obj>)
endforeach()
target_link_libraries(blib_test_all PRIVATE blib-core)
set_target_properties(blib_test_all PROPERTIES FOLDER "blib/tests")
add_test(NAME blib_test_all COMMAND blib_test_all)

# Агрегирующий таргет: собирает все тесты одной командой
# (удобно для скриптов: cmake --build <dir> --target blib_tests)
add_custom_target(blib_tests DEPENDS ${blib_test_group_exes} blib_test_all)
set_target_properties(blib_tests PROPERTIES FOLDER "blib/tests")
