# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation

#
# functions.cmake - helper variables and functions for tests/CMakeLists.txt:
#	- finding packages,
#	- building and adding test cases.
#

set(TESTS_ROOT_DIR ${PMEMSTREAM_ROOT_DIR}/tests)

set(GLOBAL_TEST_ARGS
	-DPERL_EXECUTABLE=${PERL_EXECUTABLE}
	-DMATCH_SCRIPT=${PROJECT_SOURCE_DIR}/tests/cmake/match
	-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM}
	-DTESTS_ROOT_DIR=${TESTS_ROOT_DIR}
	-DTESTS_EXECUTION_DIR=${TEST_DIR})

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

# XXX: add for libpmemset/libpmem ?
set(INCLUDE_DIRS common/ ../src ../src/include)
#set(LIBS_DIRS ${})

include_directories(${INCLUDE_DIRS})
link_directories(${LIBS_DIRS})

# ----------------------------------------------------------------- #
## Functions to find packages in the system
# ----------------------------------------------------------------- #
function(find_gdb)
	find_program(GDB gdb)
	if(GDB)
		set(ENV{PATH} ${PMDK_BIN_PREFIX}:$ENV{PATH})
		message(STATUS "Found gdb: ${GDB}")
	else()
		message(WARNING "gdb not found, some tests will be skipped.")
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

# Checks if listed dependencies are present in the system
function(check_test_dependecies)
	set(oneValueArgs NAME)
	set(multiValueArgs DEPENDENCIES)

	cmake_parse_arguments(TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	foreach(dependency ${TEST_DEPENDENCIES})
		find_program(${dependency}_PROGRAM ${dependency})
		if(NOT ${dependency}_PROGRAM)
			message(WARNING "${dependency} not found, some tests will be skipped.")
			return()
		else()
			message(STATUS "Found ${dependency}")
		endif()
	endforeach()
	set(${TEST_NAME}_FOUND 1 PARENT_SCOPE)
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

	if(${TEST_NAME} MATCHES "posix$" AND WIN32)
		return()
	endif()

	build_test(${TEST_NAME} ${TEST_SRC_FILES})
	target_link_libraries(${TEST_NAME} ${TEST_LIBS})
	target_compile_definitions(${TEST_NAME} PRIVATE ${TEST_BUILD_OPTIONS})
	target_compile_options(${TEST_NAME} PRIVATE ${TEST_OPTS})
endfunction()

# wrapper for RapidCheck tests; passes all params to build_test_ext with LIBS extended by "rapidcheck"
function(build_test_rc)
	set(oneValueArgs NAME)
	set(multiValueArgs SRC_FILES LIBS BUILD_OPTIONS OPTS)
	cmake_parse_arguments(TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	list(APPEND TEST_LIBS rapidcheck)
	build_test_ext(NAME ${TEST_NAME} SRC_FILES ${TEST_SRC_FILES} LIBS ${TEST_LIBS}
					BUILD_OPTIONS ${TEST_BUILD_OPTIONS} OPTS ${TEST_OPTS})
endfunction()

# Prepares test executable with given {name} and extra arguments equal to source files({ARGN}).
function(build_test name)
	if(${name} MATCHES "posix$" AND WIN32)
		return()
	endif()

	set(srcs ${ARGN})
	prepend(srcs ${TESTS_ROOT_DIR} ${srcs})

	add_executable(${name} ${srcs})
	target_link_libraries(${name} ${CMAKE_THREAD_LIBS_INIT} ${LIBPMEM2_LIBRARIES} pmemstream test_backtrace stream_span_helpers valgrind_internal)
	if(LIBUNWIND_FOUND)
		target_link_libraries(${name} ${LIBUNWIND_LIBRARIES} ${CMAKE_DL_LIBS})
	endif()
	if(WIN32)
		target_link_libraries(${name} dbghelp)
	endif()

	# build it, when building 'tests'
	add_dependencies(tests ${name})
endfunction()

# Configures ctest's testcase with the name: ${name}_${testcase}_${tracer}.
# It passes CMake params, including GLOBAL_TEST_ARGS and test specific args, like
# cmake_script (to run the test), name, test case number, tracer, and dirs.
# XXX: SRC_DIR is wrong, because we add tests with srcs like "api_c/testname.c"
#	   and we get actually parent dir (e.g. "../tests" instead of "../tests/api_c").
#	   We could solve this with per-dir CMake files (i.a. add CMake in api_c dir)
function(add_testcase executable name tracer testcase cmake_script)
	add_test(NAME ${name}_${testcase}_${tracer}
			COMMAND ${CMAKE_COMMAND}
			${GLOBAL_TEST_ARGS}
			-DEXECUTABLE=$<TARGET_FILE:${executable}>
			-DTEST_NAME=${name}_${testcase}_${tracer}
			-DTESTCASE=${testcase}
			-DTRACER=${tracer}
			-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
			-DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}_${testcase}_${tracer}
			${ARGN}
			-P ${cmake_script})

	set_tests_properties(${name}_${testcase}_${tracer} PROPERTIES
			ENVIRONMENT "LC_ALL=C;PATH=$ENV{PATH};"
			FAIL_REGULAR_EXPRESSION Sanitizer)

	# XXX: if we use FATAL_ERROR in test.cmake - pmemcheck passes anyway
	# workaround: look for "CMake Error" in output and fail if found
	if (${tracer} STREQUAL pmemcheck)
		set_tests_properties(${name}_${testcase}_${tracer} PROPERTIES
				FAIL_REGULAR_EXPRESSION "CMake Error")
	endif()

	if (${tracer} STREQUAL pmemcheck)
		set_tests_properties(${name}_${testcase}_${tracer} PROPERTIES
				COST 100)
	elseif(${tracer} IN_LIST vg_tracers)
		set_tests_properties(${name}_${testcase}_${tracer} PROPERTIES
				COST 50)
	else()
		set_tests_properties(${name}_${testcase}_${tracer} PROPERTIES
				COST 10)
	endif()
endfunction()

# Skipped test cases execute nothing and contain skip message in the name.
function(skip_test name message)
	add_test(NAME ${name}_${message}
		COMMAND ${CMAKE_COMMAND} -P ${TESTS_ROOT_DIR}/cmake/true.cmake)

	set_tests_properties(${name}_${message} PROPERTIES COST 0)
endfunction()

# Adds testcase if all checks passes, e.g., tracer is found, executable (target) is built, etc.
#	It skips otherwise and prints message if needed.
function(add_test_common executable name tracer testcase cmake_script)
	if(${tracer} STREQUAL "")
	    set(tracer none)
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

		# skip all valgrind tests on Windows
		if (WIN32)
			return()
		endif()

		# skip valgrind tests when measuring code coverage
		if (COVERAGE)
			return()
		endif()
	endif()

	# check if test was built
	if (NOT TARGET ${executable})
		message(WARNING "${executable} not built. Skipping.")
		return()
	endif()

	add_testcase(${executable} ${name} ${tracer} ${testcase} ${cmake_script} ${ARGN})
endfunction()

# Adds testcase with optional SCRIPT and CASE parameters.
#	If not set, use default test script and set TEST_CASE to 0.
function(add_test_generic)
	set(oneValueArgs EXECUTABLE NAME CASE SCRIPT)
	set(multiValueArgs TRACERS)
	cmake_parse_arguments(TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if("${TEST_SCRIPT}" STREQUAL "")
		if("${TEST_CASE}" STREQUAL "")
			set(TEST_CASE "0")
			set(cmake_script ${TESTS_ROOT_DIR}/cmake/run_default.cmake)
		else()
			# XXX: perhaps we'll have to fix this when we establish hierarchy, e.g.
			#	add sep. functions like this, to setup proper TEST_SCRIPT for a grup of tests?
			set(cmake_script ${SRC_DIR}/${TEST_NAME}_${TEST_CASE}.cmake)
		endif()
	else()
		if("${TEST_CASE}" STREQUAL "")
			set(TEST_CASE "0")
		endif()
		set(cmake_script ${TESTS_ROOT_DIR}/${TEST_SCRIPT})
	endif()

	if("${TEST_EXECUTABLE}" STREQUAL "")
		set(TEST_EXECUTABLE ${TEST_NAME})
	endif()

	foreach(tracer ${TEST_TRACERS})
		add_test_common(${TEST_EXECUTABLE} ${TEST_NAME} ${tracer} ${TEST_CASE} ${cmake_script})
	endforeach()
endfunction()
