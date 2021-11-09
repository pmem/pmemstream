# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

#
# tests/CMakeLists.txt - prepares all tests; it specifies which tests are enabled based on options
#	and available programs. Build tests with 'make tests' command, execute them using 'ctest'.
#
include(cmake/functions.cmake)

# ----------------------------------------------------------------- #
## Setup
# ----------------------------------------------------------------- #
add_custom_target(tests)

# XXX: valgrind
# # Find valgrind
# if(PKG_CONFIG_FOUND)
# 	pkg_check_modules(VALGRIND QUIET valgrind)
# else()
# 	find_package(VALGRIND QUIET)
# endif()

if(NOT VALGRIND_FOUND AND TESTS_USE_VALGRIND)
	message(FATAL_ERROR "Valgrind not found, but flag TESTS_USE_VALGRIND was set.")
elseif(NOT VALGRIND_FOUND)
	message(WARNING "Valgrind not found. Valgrind tests will not be performed.")
elseif(VALGRIND_FOUND)
	message(STATUS "Found Valgrind in '${VALGRIND_LIBRARY_DIRS}' (version: ${VALGRIND_VERSION})")
endif()

# XXX: libpmem2
# # Find libpmem & libpmemobj
# if(PKG_CONFIG_FOUND)
# 	pkg_check_modules(LIBPMEMOBJ REQUIRED libpmemobj>=${LIBPMEMOBJ_REQUIRED_VERSION})
# 	pkg_check_modules(LIBPMEM REQUIRED libpmem>=${LIBPMEM_REQUIRED_VERSION})
# else()
# 	find_package(LIBPMEMOBJ REQUIRED ${LIBPMEMOBJ_REQUIRED_VERSION})
# 	find_package(LIBPMEM REQUIRED ${LIBPMEM_REQUIRED_VERSION})
# endif()

# find_pmemcheck()
find_libunwind()
# find_pmreorder()
# find_pmempool()
# find_gdb()

# Add checks when DEVELOPER_MODE is ON
add_cppstyle(tests ${TEST_ROOT_DIR}/*.[chp])
add_check_whitespace(tests ${TEST_ROOT_DIR}/*.*
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.*)

add_library(test_backtrace STATIC common/test_backtrace.c)
if(LIBUNWIND_FOUND)
	target_compile_definitions(test_backtrace PUBLIC USE_LIBUNWIND=1)
endif()

# XXX: add libpmem dependency
# add_executable(check_is_pmem common/check_is_pmem.cpp)
# target_link_libraries(check_is_pmem ${LIBPMEM_LIBRARIES})

if(COVERAGE AND VALGRIND_FOUND)
	message(STATUS "This is the Coverage build, skipping Valgrind tests")
endif()

# ----------------------------------------------------------------- #
## Common tests
# ----------------------------------------------------------------- #

# XXX: add first test
# build_test(XYZ XYZ.c)
# add_test_generic(NAME XYZ TRACERS none)
