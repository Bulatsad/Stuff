# ============================================================
# beng tests (по образцу blib/test)
#
# Каждая группа компилируется один раз как OBJECT-библиотека и
# линкуется в свой executable и в агрегатный beng_test_all.
# OBJECT-библиотеки не режутся MSVC-линкером (в отличие от static
# libs), поэтому самрегистрация BLIB_TEST_CASE работает без
# /WHOLEARCHIVE.
#
# main.cpp (BLIB_TEST_MAIN) переиспользуется из blib/test через
# OBJECT-таргет blib_test_main.
# ============================================================

# Список групп: имя группы == суффикс файла test<group>.cpp
set(beng_test_groups registry componentPool scene entity transform time)

# Список standalone-исполняемых групп (заполняется в цикле ниже)
set(beng_test_group_exes)

foreach(group ${beng_test_groups})
	# OBJECT library holding the group's tests
	set(group_obj beng_test_${group}_obj)
	add_library(${group_obj} OBJECT src/impl/test${group}.cpp)
	target_include_directories(${group_obj} PRIVATE ${root_dir})
	target_link_libraries(${group_obj} PRIVATE beng-core)
	set_target_properties(${group_obj} PROPERTIES FOLDER "beng/tests/${group}")

	# Standalone executable for this group: main + group objects
	set(group_exe beng_test_${group})
	add_executable(${group_exe})
	target_sources(${group_exe} PRIVATE
		$<TARGET_OBJECTS:blib_test_main>
		$<TARGET_OBJECTS:${group_obj}>
	)
	target_link_libraries(${group_exe} PRIVATE beng-core)
	set_target_properties(${group_exe} PROPERTIES FOLDER "beng/tests/${group}")
	add_test(NAME ${group_exe} COMMAND ${group_exe})
	list(APPEND beng_test_group_exes ${group_exe})
endforeach()

# Агрегатный executable: все группы в одном процессе
# (удобство — падение одной группы валит весь прогон)
add_executable(beng_test_all)
target_sources(beng_test_all PRIVATE $<TARGET_OBJECTS:blib_test_main>)
foreach(group ${beng_test_groups})
	target_sources(beng_test_all PRIVATE $<TARGET_OBJECTS:beng_test_${group}_obj>)
endforeach()
target_link_libraries(beng_test_all PRIVATE beng-core)
set_target_properties(beng_test_all PROPERTIES FOLDER "beng/tests")
add_test(NAME beng_test_all COMMAND beng_test_all)

# Агрегирующий таргет: собирает все тесты одной командой
add_custom_target(beng_tests DEPENDS ${beng_test_group_exes} beng_test_all)
set_target_properties(beng_tests PROPERTIES FOLDER "beng/tests")
