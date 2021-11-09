# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021, Intel Corporation

#
# functions.cmake - helper functions for tests/CMakeLists.txt:
#	- finding packages,
#	- building and adding test cases.
#

set(TEST_ROOT_DIR ${PMEMSTREAM_ROOT_DIR}/tests)

set(GLOBAL_TEST_ARGS
	-DPERL_EXECUTABLE=${PERL_EXECUTABLE}
	-DMATCH_SCRIPT=${PROJECT_SOURCE_DIR}/tests/match
	-DPARENT_DIR=${TEST_DIR}
	-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM}
	-DTEST_ROOT_DIR=${TEST_ROOT_DIR})

if(TRACE_TESTS)
	set(GLOBAL_TEST_ARGS ${GLOBAL_TEST_ARGS} --trace-expand)
endif()

# List of supported Valgrind tracers
set(vg_tracers memcheck helgrind drd pmemcheck)

# Set dir where PMDK tools' binaries are held
set(PMDK_BIN_PREFIX ${LIBPMEM2_PREFIX}/bin)

# ----------------------------------------------------------------- #
## Include and link required dirs/libs for tests
# ----------------------------------------------------------------- #

# XXX: add for libpmem2/libpmemset/libpmem (?) # ${LIBPMEMOBJ++_INCLUDE_DIRS}
set(INCLUDE_DIRS ../common/ ../../src)
# set(LIBS_DIRS ${LIBPMEMOBJ++_LIBRARY_DIRS})

include_directories(${INCLUDE_DIRS})
link_directories(${LIBS_DIRS})

# ----------------------------------------------------------------- #
## Functions to find packages in the system
# ----------------------------------------------------------------- #
function(find_gdb)
	execute_process(COMMAND gdb --help
			RESULT_VARIABLE GDB_RET
			OUTPUT_QUIET
			ERROR_QUIET)
	if(GDB_RET)
		set(GDB_FOUND 0 CACHE INTERNAL "")
		message(WARNING "gdb not found, some tests will be skipped")
	else()
		set(GDB_FOUND 1 CACHE INTERNAL "")
		message(STATUS "Found gdb")
	endif()
endfunction()

function(find_pmemcheck)
	if(WIN32)
		return()
	endif()

	if(NOT VALGRIND_FOUND)
		return()
	endif()

	set(ENV{PATH} ${VALGRIND_PREFIX}/bin:$ENV{PATH})
	execute_process(COMMAND valgrind --tool=pmemcheck --help
			RESULT_VARIABLE VALGRIND_PMEMCHECK_RET
			OUTPUT_QUIET
			ERROR_QUIET)
	if(VALGRIND_PMEMCHECK_RET)
		set(VALGRIND_PMEMCHECK_FOUND 0 CACHE INTERNAL "")
	else()
		set(VALGRIND_PMEMCHECK_FOUND 1 CACHE INTERNAL "")
	endif()

	if(VALGRIND_PMEMCHECK_FOUND)
		execute_process(COMMAND valgrind --tool=pmemcheck true
				ERROR_VARIABLE PMEMCHECK_OUT
				OUTPUT_QUIET)

		string(REGEX MATCH ".*pmemcheck-([0-9.]+),.*" PMEMCHECK_OUT "${PMEMCHECK_OUT}")
		set(PMEMCHECK_VERSION ${CMAKE_MATCH_1} CACHE INTERNAL "")
		message(STATUS "Valgrind pmemcheck found, version: ${PMEMCHECK_VERSION}")
	else()
		message(WARNING "Valgrind pmemcheck not found. Pmemcheck tests will not be performed.")
	endif()
endfunction()

function(find_libunwind)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(LIBUNWIND QUIET libunwind)
	else()
		find_package(LIBUNWIND QUIET)
	endif()

	if(NOT LIBUNWIND_FOUND)
		message(WARNING "libunwind not found. Stack traces from tests will not be reliable")
	else()
		message(STATUS "Found libunwind, version ${LIBUNWIND_VERSION}")
	endif()
endfunction()

function(find_pmreorder)
	if(NOT VALGRIND_FOUND OR NOT VALGRIND_PMEMCHECK_FOUND)
		message(WARNING "Pmreorder will not be used. Valgrind with pmemcheck must be installed")
		return()
	endif()

	if((NOT(PMEMCHECK_VERSION VERSION_LESS 1.0)) AND PMEMCHECK_VERSION VERSION_LESS 2.0)
		find_program(PMREORDER names pmreorder HINTS ${PMDK_BIN_PREFIX})

		if(PMREORDER)
			get_program_version_major_minor(${PMREORDER} PMREORDER_VERSION)
			message(STATUS "Found pmreorder: ${PMREORDER}, in version: ${PMREORDER_VERSION}")

			set(ENV{PATH} ${PMDK_BIN_PREFIX}:$ENV{PATH})
			set(PMREORDER_SUPPORTED true CACHE INTERNAL "pmreorder support")
		else()
			message(WARNING "Pmreorder not found - pmreorder tests will not be performed.")
		endif()
	else()
		message(WARNING "Pmemcheck must be installed in version 1.X for pmreorder to work - pmreorder tests will not be performed.")
	endif()
endfunction()

function(find_pmempool)
	# XXX: fix and use to libpmem2 prefix
	find_program(PMEMPOOL names pmempool HINTS ${PMDK_BIN_PREFIX})
	if(PMEMPOOL)
		set(ENV{PATH} ${PMDK_BIN_PREFIX}:$ENV{PATH})
		message(STATUS "Found pmempool: ${PMEMPOOL}")
	else()
		message(FATAL_ERROR "Pmempool not found.")
	endif()
endfunction()

# ----------------------------------------------------------------- #
## Functions for building and adding new testcases
# ----------------------------------------------------------------- #

# Function to build test with custom build options (e.g. passing defines),
# link it with custom library/-ies and compile options. It calls build_test function.
# Usage: build_test_ext(NAME .. SRC_FILES .. .. LIBS .. .. BUILD_OPTIONS .. .. OPTS .. ..)
function(build_test_ext)
	set(oneValueArgs NAME)
	set(multiValueArgs SRC_FILES LIBS BUILD_OPTIONS OPTS)
	cmake_parse_arguments(TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	set(LIBS_TO_LINK "")

	if(${TEST_NAME} MATCHES "posix$" AND WIN32)
		return()
	endif()

	foreach(lib ${TEST_LIBS})
		# XXX: add support for libpmem2?
		if("${lib}" STREQUAL "libpmemobj_cpp")
			if("${LIBPMEMOBJ++_LIBRARIES}" STREQUAL "")
				return()
			else()
				list(APPEND LIBS_TO_LINK ${LIBPMEMOBJ++_LIBRARIES})
			endif()
		elseif("${lib}" STREQUAL "dl_libs")
			list(APPEND LIBS_TO_LINK ${CMAKE_DL_LIBS})
		endif()
	endforeach()

	build_test(${TEST_NAME} ${TEST_SRC_FILES})
	target_link_libraries(${TEST_NAME} ${LIBS_TO_LINK})
	target_compile_definitions(${TEST_NAME} PRIVATE ${TEST_BUILD_OPTIONS})
	target_compile_options(${TEST_NAME} PRIVATE ${TEST_OPTS})
endfunction()

function(build_test name)
	if(${name} MATCHES "posix$" AND WIN32)
		return()
	endif()

	set(srcs ${ARGN})
	prepend(srcs ${TEST_ROOT_DIR} ${srcs})

	add_executable(${name} ${srcs})
	# XXX: libpmem2? ${LIBPMEMOBJ_LIBRARIES}
	target_link_libraries(${name} ${CMAKE_THREAD_LIBS_INIT} pmemstream test_backtrace)
	if(LIBUNWIND_FOUND)
		target_link_libraries(${name} ${LIBUNWIND_LIBRARIES} ${CMAKE_DL_LIBS})
	endif()
	if(WIN32)
		target_link_libraries(${name} dbghelp)
	endif()

	add_dependencies(tests ${name})
endfunction()

# Configures testcase ${test_name}_${testcase} with ${tracer}
# and cmake_script used to execute test
function(add_testcase test_name tracer testcase cmake_script)
	add_test(NAME ${test_name}_${testcase}_${tracer}
			COMMAND ${CMAKE_COMMAND}
			${GLOBAL_TEST_ARGS}
			-DTEST_NAME=${test_name}_${testcase}_${tracer}
			-DTESTCASE=${testcase}
			-DPARENT_SRC_DIR=${TEST_ROOT_DIR}
			-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
			-DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}/${test_name}_${testcase}_${tracer}
			-DTEST_EXECUTABLE=$<TARGET_FILE:${test_name}>
			-DTRACER=${tracer}
			-DLONG_TESTS=${LONG_TESTS}
			${ARGN}
			-P ${cmake_script})

	set_tests_properties(${test_name}_${testcase}_${tracer} PROPERTIES
			ENVIRONMENT "LC_ALL=C;PATH=$ENV{PATH};"
			FAIL_REGULAR_EXPRESSION Sanitizer)

	# XXX: if we use FATAL_ERROR in test.cmake - pmemcheck passes anyway
	# workaround: look for "CMake Error" in output and fail if found
	if (${tracer} STREQUAL pmemcheck)
		set_tests_properties(${test_name}_${testcase}_${tracer} PROPERTIES
				FAIL_REGULAR_EXPRESSION "CMake Error")
	endif()

	if (${tracer} STREQUAL pmemcheck)
		set_tests_properties(${test_name}_${testcase}_${tracer} PROPERTIES
				COST 100)
	elseif(${tracer} IN_LIST vg_tracers)
		set_tests_properties(${test_name}_${testcase}_${tracer} PROPERTIES
				COST 50)
	else()
		set_tests_properties(${test_name}_${testcase}_${tracer} PROPERTIES
				COST 10)
	endif()
endfunction()

function(skip_test name message)
	add_test(NAME ${name}_${message}
		COMMAND ${CMAKE_COMMAND} -P ${TEST_ROOT_DIR}/true.cmake)

	set_tests_properties(${name}_${message} PROPERTIES COST 0)
endfunction()

# adds testcase only if tracer is found and target is build, skips otherwise
function(add_test_common test_name tracer testcase cmake_script)
	if(${tracer} STREQUAL "")
	    set(tracer none)
	endif()

	# skip all Valgrind tests on Windows
	if(WIN32 AND ${tracer} IN_LIST vg_tracers)
		return()
	endif()

	if(${tracer} IN_LIST vg_tracers)
		if(TESTS_USE_VALGRIND)
			# pmemcheck is not necessarily required (because it's not always delivered with Valgrind)
			if(${tracer} STREQUAL "pmemcheck" AND (NOT VALGRIND_PMEMCHECK_FOUND))
				skip_test(${name}_${testcase}_${tracer} "SKIPPED_BECAUSE_OF_MISSING_PMEMCHECK")
				return()
			# Valgrind on the other hand is required, because we enabled Valgrind tests
			elseif(NOT VALGRIND_FOUND)
				message(FATAL_ERROR "Valgrind not found, but tests are enabled. To disable them set CMake option TESTS_USE_VALGRIND=OFF.")
			endif()
		else()
			# TESTS_USE_VALGRIND=OFF so just don't run Valgrind tests
			return()
		endif()

		if(USE_ASAN OR USE_UBSAN)
			skip_test(${name}_${testcase}_${tracer} "SKIPPED_BECAUSE_SANITIZER_USED")
			return()
		endif()
	endif()

	# if test was not build
	if (NOT TARGET ${test_name})
		message(WARNING "${test_name} not build. Skipping.")
		return()
	endif()

	# skip all valgrind tests on windows
	if ((NOT ${tracer} STREQUAL none) AND WIN32)
		return()
	endif()

	if (COVERAGE AND ${tracer} IN_LIST vg_tracers)
		return()
	endif()

	add_testcase(${test_name} ${tracer} ${testcase} ${cmake_script} ${ARGN})
endfunction()

# adds testcase with optional SCRIPT and TEST_CASE parameters
function(add_test_generic)
	set(oneValueArgs NAME CASE SCRIPT)
	set(multiValueArgs TRACERS)
	cmake_parse_arguments(TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if("${TEST_SCRIPT}" STREQUAL "")
		if("${TEST_CASE}" STREQUAL "")
			set(TEST_CASE "0")
			set(cmake_script ${TEST_ROOT_DIR}/cmake/run_default.cmake)
		else()
			# XXX: need to fix when we establish hierarchy,
			#	e.g., like rpma? sub-dirs "unit", "integration", and "MT"?
			# perhaps just use SRC_DIR variable
			set(cmake_script ${TEST_ROOT_DIR}/${TEST_NAME}/${TEST_NAME}_${TEST_CASE}.cmake)
		endif()
	else()
		if("${TEST_CASE}" STREQUAL "")
			set(TEST_CASE "0")
		endif()
		set(cmake_script ${TEST_ROOT_DIR}/${TEST_SCRIPT})
	endif()

	foreach(tracer ${TEST_TRACERS})
		add_test_common(${TEST_NAME} ${TEST_NAME} ${tracer} ${TEST_CASE} ${cmake_script})
	endforeach()
endfunction()
