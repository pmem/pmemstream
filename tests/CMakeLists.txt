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

set(CXX_STANDARD 17 CACHE STRING "C++ language standard")

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD ${CXX_STANDARD})

# Finds valgrind and checks for pmemcheck's availability
 if(PKG_CONFIG_FOUND)
	pkg_check_modules(VALGRIND QUIET valgrind)
else()
	find_package(VALGRIND QUIET)
endif()

if(NOT VALGRIND_FOUND AND TESTS_USE_VALGRIND)
	message(FATAL_ERROR "Valgrind not found, but flag TESTS_USE_VALGRIND was set.")
elseif(NOT VALGRIND_FOUND)
	message(WARNING "Valgrind not found. Valgrind tests will not be performed.")
elseif(VALGRIND_FOUND)
	message(STATUS "Found Valgrind in '${VALGRIND_LIBRARY_DIRS}' (version: ${VALGRIND_VERSION})")

	include_directories(${VALGRIND_INCLUDE_DIRS})
	find_pmemcheck()
endif()

find_libunwind()
# find_pmreorder()
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
build_test(stream_from_map api_c/stream_from_map.c)
add_test_generic(NAME stream_from_map TRACERS none memcheck pmemcheck drd helgrind)

build_test_ext(NAME id_manager_multithreaded SRC_FILES unittest/id_manager_multithreaded.cpp ../src/id_manager.c ../src/critnib/critnib.c)
add_test_generic(NAME id_manager_multithreaded TRACERS none memcheck drd helgrind)

if(TESTS_RAPIDCHECK)
	build_test_rc(NAME append SRC_FILES unittest/append.cpp)
	add_test_generic(NAME append TRACERS none)

	build_test_rc(NAME concurrent_iterate SRC_FILES unittest/concurrent_iterate.cpp)
	add_test_generic(NAME concurrent_iterate TRACERS none)

	build_test_rc(NAME create SRC_FILES unittest/create.cpp)
	add_test_generic(NAME create TRACERS none)

	build_test_rc(NAME get_region_runtime SRC_FILES unittest/get_region_runtime.cpp)
	add_test_generic(NAME get_region_runtime TRACERS none)

	build_test_rc(NAME id_manager SRC_FILES unittest/id_manager.cpp ../src/id_manager.c ../src/critnib/critnib.c)
	add_test_generic(NAME id_manager TRACERS none memcheck)

	build_test_rc(NAME reserve_publish SRC_FILES unittest/reserve_publish.cpp)
	add_test_generic(NAME reserve_publish TRACERS none memcheck pmemcheck)

	build_test_rc(NAME util_popcount SRC_FILES unittest/util_popcount.cpp)
	add_test_generic(NAME util_popcount TRACERS none)
endif()

if(GDB AND DEBUG_BUILD)
	build_test(append_break integrity/append_break.cpp)
	add_test_generic(NAME append_break SCRIPT integrity/append_break.cmake TRACERS none)
endif()

check_test_dependecies(NAME symbols_dependencies DEPENDENCIES bash awk grep objdump)
if(symbols_dependencies_FOUND)
	add_test(NAME symbols COMMAND bash ${TESTS_ROOT_DIR}/abi/check_symbols.sh
		${PMEMSTREAM_ROOT_DIR}/src/libpmemstream.map ${CMAKE_BINARY_DIR}/src/libpmemstream.so)
endif()

# ----------------------------------------------------------------- #
## Tests using examples
# ----------------------------------------------------------------- #
if(BUILD_EXAMPLES)
	add_dependencies(tests
				example-01_basic_iterate
				example-02_visual_iterator)
	add_test_generic(NAME example-01_basic_iterate TRACERS none)
	add_test_generic(NAME example-02_visual_iterator TRACERS none)
endif()
