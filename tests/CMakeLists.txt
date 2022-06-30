# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

#
# tests/CMakeLists.txt - prepares all tests; it specifies which tests are enabled based on options
#	and available programs. Build tests with 'make tests' command, execute them using 'ctest'.
#
include(cmake/functions.cmake)

# ----------------------------------------------------------------- #
## Setup
# ----------------------------------------------------------------- #
add_custom_target(tests)

# Finds valgrind and checks for pmemcheck's availability
 if(PKG_CONFIG_FOUND)
	pkg_check_modules(VALGRIND QUIET valgrind)
else()
	find_package(VALGRIND QUIET)
endif()

if(NOT VALGRIND_FOUND AND TESTS_USE_VALGRIND)
	message(FATAL_ERROR "Valgrind not found, but flag TESTS_USE_VALGRIND was set.")
elseif(NOT VALGRIND_FOUND)
	message(STATUS "Valgrind not found. Valgrind tests will not be performed.")
elseif(VALGRIND_FOUND)
	message(STATUS "Found Valgrind in '${VALGRIND_LIBRARY_DIRS}' (version: ${VALGRIND_VERSION})")

	include_directories(${VALGRIND_INCLUDE_DIRS})
	find_pmemcheck()
endif()

find_libunwind()
find_pmreorder()
find_gdb()

if(TESTS_RAPIDCHECK)
	find_package(rapidcheck REQUIRED)
endif()

# Add checks when DEVELOPER_MODE is ON
add_cppstyle(tests ${TESTS_ROOT_DIR}/*/*.[ch]
		   ${TESTS_ROOT_DIR}/*/*.[ch]pp)
add_check_whitespace(tests ${TESTS_ROOT_DIR}/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.*)

add_library(test_backtrace STATIC common/test_backtrace.c)
if(LIBUNWIND_FOUND)
	target_compile_definitions(test_backtrace PUBLIC USE_LIBUNWIND=1)
endif()
add_library(stream_span_helpers STATIC common/stream_span_helpers.cpp ../src/span.c)
add_library(valgrind_internal STATIC common/valgrind_internal.c)

# Set variable to know if debug tests can be run
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(DEBUG_BUILD 1)
endif()

# XXX: add libpmemset dependency and fix 'check_is_pmem.cpp'
#		or use libpmem library? (fix findLIBPMEM2)
# add_executable(check_is_pmem common/check_is_pmem.cpp)
# target_link_libraries(check_is_pmem ${LIBPMEM_LIBRARIES})

# Produce general warnings, e.g. when a command/tool is missing
if(NOT DEBUG_BUILD)
	message(STATUS "This is a non-debug build - some tests may be skipped.")
endif()

if(NOT GDB)
	message(WARNING "GDB was not found - skipping some tests. To fix, make sure `gdb` command is OS-wide accessible.")
endif()

if(COVERAGE AND TESTS_USE_VALGRIND)
	message(STATUS "This is the Coverage build - skipping Valgrind tests")
endif()

# ----------------------------------------------------------------- #
## All tests
# ----------------------------------------------------------------- #
build_test_ext(NAME async SRC_FILES api_c/async.c LIBS miniasync)
add_test_generic(NAME async TRACERS none memcheck pmemcheck drd helgrind)

build_test(append_entry api_c/append_entry.c)
add_test_generic(NAME append_entry TRACERS none memcheck pmemcheck drd helgrind)

build_test(entry_iterator api_c/entry_iterator.c)
add_test_generic(NAME entry_iterator TRACERS none memcheck pmemcheck drd helgrind)

build_test(region_create api_c/region_create.c)
add_test_generic(NAME region_create TRACERS none memcheck pmemcheck drd helgrind)

build_test(region_iterator api_c/region_iterator.c)
add_test_generic(NAME region_iterator TRACERS none memcheck pmemcheck drd helgrind)

build_test(reserve_and_publish api_c/reserve_and_publish.c)
add_test_generic(NAME reserve_and_publish TRACERS none memcheck pmemcheck drd helgrind)

build_test(stream_from_map api_c/stream_from_map.c)
add_test_generic(NAME stream_from_map TRACERS none memcheck pmemcheck drd helgrind)

build_test(timestamp_api api_c/timestamp.c)
add_test_generic(NAME timestamp_api TRACERS none memcheck pmemcheck drd helgrind)

if(TESTS_RAPIDCHECK)
	build_test_rc(NAME append SRC_FILES unittest/append.cpp LIBS miniasync)
	add_test_generic(NAME append TRACERS none memcheck pmemcheck)

	build_test_rc(NAME append_oom SRC_FILES unittest/append_oom.cpp LIBS miniasync)
	add_test_generic(NAME append_oom TRACERS none)

	build_test_rc(NAME concurrent_async_wait SRC_FILES unittest/concurrent_async_wait.cpp LIBS miniasync)
	# XXX: enable drd and helgrind
	add_test_generic(NAME concurrent_async_wait TRACERS none memcheck pmemcheck)

	build_test_rc(NAME concurrent_iterate SRC_FILES unittest/concurrent_iterate.cpp LIBS miniasync)
	add_test_generic(NAME concurrent_iterate TRACERS none)

	build_test_rc(NAME concurrent_iterate_with_append SRC_FILES unittest/concurrent_iterate_with_append.cpp LIBS miniasync)
	add_test_generic(NAME concurrent_iterate_with_append TRACERS none)

	build_test_rc(NAME create SRC_FILES unittest/create.cpp LIBS miniasync)
	add_test_generic(NAME create TRACERS none)

	build_test_rc(NAME multi_region SRC_FILES unittest/multi_region.cpp LIBS miniasync)
	add_test_generic(NAME multi_region TRACERS none memcheck pmemcheck)

	build_test_rc(NAME multi_region_state SRC_FILES unittest/multi_region_state.cpp LIBS miniasync)
	add_test_generic(NAME multi_region_state TRACERS none)

	build_test_rc(NAME publish_append_async SRC_FILES unittest/publish_append_async.cpp LIBS miniasync)
	add_test_generic(NAME publish_append_async TRACERS none)

	build_test_rc(NAME region_runtime_initialize SRC_FILES unittest/region_runtime_initialize.cpp ../src/region.c ../src/critnib/critnib.c ../src/iterator.c LIBS miniasync)
	add_test_generic(NAME region_runtime_initialize TRACERS none memcheck)

	build_test_rc(NAME reserve_publish SRC_FILES unittest/reserve_publish.cpp LIBS miniasync)
	add_test_generic(NAME reserve_publish TRACERS none memcheck pmemcheck)

	build_test_rc(NAME reserve_publish_integrity SRC_FILES integrity/reserve_publish.cpp LIBS miniasync)
	add_test_generic(NAME reserve_publish_integrity TRACERS none memcheck)

	build_test_rc(NAME singly_linked_list SRC_FILES unittest/singly_linked_list.cpp)
	add_test_generic(NAME singly_linked_list TRACERS none memcheck)

	build_test_rc(NAME singly_linked_list_state SRC_FILES unittest/singly_linked_list_state.cpp)
	add_test_generic(NAME singly_linked_list_state TRACERS none memcheck)

	build_test_rc(NAME timestamp SRC_FILES unittest/timestamp.cpp LIBS miniasync)
	add_test_generic(NAME timestamp TRACERS none memcheck pmemcheck)

	build_test_rc(NAME util_common SRC_FILES unittest/util_common.cpp)
	add_test_generic(NAME util_common TRACERS none)

	build_test_rc(NAME iterate_all SRC_FILES layout/iterate_all.cpp ../src/span.c LIBS miniasync)
	add_test_generic(NAME iterate_all TRACERS none)

	build_test_rc(NAME iterate_validation SRC_FILES layout/iterate_validation.cpp ../src/span.c LIBS miniasync)
	add_test_generic(NAME iterate_validation TRACERS none memcheck pmemcheck)
endif()

if(TESTS_PMREORDER)
	build_test_ext(NAME singly_linked_list_pmreorder SRC_FILES integrity/singly_linked_list_pmreorder.cpp LIBS miniasync)
	add_test_generic(NAME singly_linked_list_pmreorder SCRIPT integrity/default_create_fill_check.cmake TRACERS none)
	add_test_generic(NAME singly_linked_list_pmreorder CASE negative SCRIPT integrity/default_create_fill_check_negative.cmake TRACERS none)

	build_test_ext(NAME multi_region_pmreorder SRC_FILES integrity/multi_region_pmreorder.cpp LIBS miniasync)
	add_test_generic(NAME multi_region_pmreorder SCRIPT integrity/default_create_fill_check.cmake TRACERS none)
endif()

if(GDB AND DEBUG_BUILD)
	build_test_ext(NAME append_break SRC_FILES integrity/append_break.cpp LIBS miniasync)
	foreach(TESTCASE RANGE 2)
		add_test_generic(NAME append_break SCRIPT integrity/append_break.cmake CASE ${TESTCASE} TRACERS none)
	endforeach()

	# test case 0 does not contain a broken entry (in the understanding of this test script)
	add_test_generic(EXECUTABLE append_break NAME append_break_lazy SCRIPT integrity/append_break_lazy.cmake CASE 1 TRACERS none)
	add_test_generic(EXECUTABLE append_break NAME append_break_lazy SCRIPT integrity/append_break_lazy.cmake CASE 2 TRACERS none)
endif()

check_test_dependecies(NAME symbols_dependencies DEPENDENCIES bash awk grep objdump)
if(symbols_dependencies_FOUND)
	add_test(NAME symbols COMMAND bash ${TESTS_ROOT_DIR}/abi/check_symbols.sh
		${PMEMSTREAM_ROOT_DIR}/src/libpmemstream.map ${CMAKE_BINARY_DIR}/src/libpmemstream.so)
endif()

build_test(thread_helpers common/thread_helpers_test.cpp)
add_test_generic(NAME thread_helpers TRACERS none)

# ----------------------------------------------------------------- #
## Tests using examples
# ----------------------------------------------------------------- #
if(BUILD_EXAMPLES)
	add_dependencies(tests
				example-01_basic_iterate
				example-02_visual_iterator
				example-03_reserve_publish
				example-04_basic_async
				example-05_timestamp_based_order)
	add_dependencies(example-02_visual_iterator example-01_basic_iterate)
	add_test_generic(NAME example-01_basic_iterate TRACERS none)
	add_test_generic(NAME example-02_visual_iterator SCRIPT unittest/run_on_existing.cmake TRACERS none)
	add_test_generic(NAME example-03_reserve_publish TRACERS none)
	add_test_generic(NAME example-04_basic_async TRACERS none)
	add_test_generic(NAME example-05_timestamp_based_order TRACERS none)
endif()

# ----------------------------------------------------------------- #
## Tests using benchmarks
# ----------------------------------------------------------------- #
if(BUILD_BENCHMARKS)
	add_dependencies(tests
				benchmark-append)
	add_test_generic(NAME benchmark-append SCRIPT benchmarks/append.cmake  TRACERS none)
endif()

