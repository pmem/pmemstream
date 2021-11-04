# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021, Intel Corporation

cmake_minimum_required(VERSION 3.5)

project(pmemstream C CXX)
set(PMEMSTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(${PMEMSTREAM_ROOT_DIR}/cmake/functions.cmake)

# ----------------------------------------------------------------- #
## Set required and useful variables
# ----------------------------------------------------------------- #
set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
#set(VERSION_PRERELEASE rc1)

set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
if(VERSION_PRERELEASE)
	set(VERSION ${VERSION}-${VERSION_PRERELEASE})
endif()

# set SRCVERSION, it's more accurate and "current" than VERSION
set_source_ver(SRCVERSION)

set(C_STANDARD 11 CACHE STRING "C language standard")

set(C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD ${C_STANDARD})

# Specify and print the build type
set(DEFAULT_BUILD_TYPE "RelWithDebInfo")
set(predefined_build_types
	Debug
	Release
	RelWithDebInfo
	MinSizeRel)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE}
		CACHE STRING "choose the type of build (${predefined_build_types})" FORCE)
	message(STATUS "CMAKE_BUILD_TYPE not set, setting the default one: ${CMAKE_BUILD_TYPE}")
else()
	message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
	if(NOT CMAKE_BUILD_TYPE IN_LIST predefined_build_types)
		message(WARNING "Unusual build type was set, please make sure it's proper one. "
			"By default supported are only following: ${predefined_build_types}.")
	endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PMEMSTREAM_ROOT_DIR}/cmake)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
# set(LIBPMEMOBJ_REQUIRED_VERSION 1.9) XXX: enable for libpmemset
# Only pmreorder in ver. >= 1.9 guarantees reliable output
set(PMREORDER_REQUIRED_VERSION 1.9)

# XXX: setup these vars with first dependencies
# set(PKG_CONFIG_REQUIRES)
# set(DEB_DEPENDS)
# set(RPM_DEPENDS)

set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test CACHE STRING "working directory for tests")
message(STATUS "TEST_DIR set to: \"${TEST_DIR}\"")

# Do not treat include directories from the interfaces
# of consumed Imported Targets as SYSTEM by default.
set(CMAKE_NO_SYSTEM_FROM_IMPORTED 1)

# ----------------------------------------------------------------- #
## CMake build options
# ----------------------------------------------------------------- #
# XXX: turn these ON, when appropriate sources added
option(BUILD_EXAMPLES "build examples" OFF)
option(BUILD_TESTS "build tests" OFF)
option(BUILD_DOC "build documentation" OFF)

option(USE_CCACHE "use ccache if it is available in the system" ON)
option(COVERAGE "enable collecting of coverage data" OFF)
option(DEVELOPER_MODE "enable developer checks" OFF)
option(CHECK_CPP_STYLE "check code style of C++ sources" OFF) # XXX: change the name to 'C_STYLE'?
option(TRACE_TESTS "more verbose test outputs" OFF)
option(USE_LIBUNWIND "use libunwind for more reliable stack traces from tests (it's disabled when running on Valgrind)" ON)
option(USE_ASAN "enable AddressSanitizer checks" OFF)
option(USE_UBSAN "enable UndefinedBehaviorSanitizer checks" OFF)

option(TESTS_USE_FORCED_PMEM "run tests with PMEM_IS_PMEM_FORCE=1 - it speeds up tests execution on emulated pmem" OFF)
option(TESTS_USE_VALGRIND "enable tests with valgrind (fail build if Valgrind not found)" ON)
option(TESTS_PMREORDER "enable tests with pmreorder (if pmreorder found; it requires PMDK ver. >= 1.9)" OFF)
option(TESTS_LONG "enable long running tests" OFF)

# ----------------------------------------------------------------- #
## Setup environment, find packages, set compiler's flags,
## add additional custom targets, ...
# ----------------------------------------------------------------- #
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
# set it up only within the source dir (and its own CMake file) ?
#include(CheckCSourceCompiles)
include(FindThreads)

if(NOT WIN32)
	find_package(PkgConfig QUIET)
endif()

# Configure the ccache as compiler launcher
find_program(CCACHE_FOUND ccache)
if(USE_CCACHE AND CCACHE_FOUND)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()

if(USE_ASAN)
	add_sanitizer_flag(address)
endif()
if(USE_UBSAN)
	add_sanitizer_flag(undefined)
endif()
if(COVERAGE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -coverage")
	# XXX: cxx for tests only?
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -coverage")
endif()

add_common_flag(-Wall)
add_common_flag(-Wmissing-variable-declarations)
add_common_flag(-Wpointer-arith)
add_common_flag(-Wsign-conversion)
add_common_flag(-Wsign-compare)
add_common_flag(-Wunreachable-code-return)
add_common_flag(-Wunused-macros)
add_common_flag(-fno-common)
add_common_flag(-ggdb DEBUG)
add_common_flag(-DDEBUG DEBUG)
add_common_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" RELEASE)

# XXX: valgrind
# Finds valgrind and checks for pmemcheck's availability
# if(PKG_CONFIG_FOUND)
# 	pkg_check_modules(VALGRIND QUIET valgrind)
# else()
# 	find_package(VALGRIND QUIET)
# endif()
#
# if(VALGRIND_FOUND)
# 	message(STATUS "Found Valgrind in '${VALGRIND_LIBRARY_DIRS}' (version: ${VALGRIND_VERSION})")
#
# 	add_flag(-DLIBPMEMOBJ_CPP_VG_MEMCHECK_ENABLED=1)
# 	add_flag(-DLIBPMEMOBJ_CPP_VG_DRD_ENABLED=1)
# 	add_flag(-DLIBPMEMOBJ_CPP_VG_HELGRIND_ENABLED=1)
#
# 	include_directories(${VALGRIND_INCLUDE_DIRS})
# 	find_pmemcheck()
#
# 	if(VALGRIND_PMEMCHECK_FOUND)
# 		add_flag(-DLIBPMEMOBJ_CPP_VG_PMEMCHECK_ENABLED=1)
# 	endif()
# elseif(NOT VALGRIND_FOUND AND TESTS_USE_VALGRIND)
# 	message(FATAL_ERROR "Valgrind not found, but flag TESTS_USE_VALGRIND was set.")
# elseif(NOT VALGRIND_FOUND)
# 	message(WARNING "Valgrind not found. Valgrind tests will not be performed.")
# endif()

# XXX: pmdk/libpmemset
# if(BUILD_TESTS OR BUILD_EXAMPLES)
# 	# Find libpmem and libpmemobj (PMDK libraries)
# 	if(PKG_CONFIG_FOUND)
# 		pkg_check_modules(LIBPMEMOBJ REQUIRED libpmemobj>=${LIBPMEMOBJ_REQUIRED_VERSION})
# 		pkg_check_modules(LIBPMEM REQUIRED libpmem>=${LIBPMEM_REQUIRED_VERSION})
# 	else()
# 		find_package(LIBPMEMOBJ REQUIRED ${LIBPMEMOBJ_REQUIRED_VERSION})
# 		find_package(LIBPMEM REQUIRED ${LIBPMEM_REQUIRED_VERSION})
# 	endif()
#
# 	# Some tests and examples require clang >= 8.0, because of the bug
# 	# (https://bugs.llvm.org/show_bug.cgi?id=28280), which is fixed in clang v8.0.
#
# 	if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang" AND "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
# 		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0")
# 			set(CLANG_DESTRUCTOR_REFERENCE_BUG_PRESENT 1)
# 		endif()
# 	endif()
# endif()

add_custom_target(checkers ALL)
add_custom_target(cppstyle) # XXX: change the name to 'cstyle'?
add_custom_target(cppformat)
add_custom_target(check-whitespace)
add_custom_target(check-license
	COMMAND ${PMEMSTREAM_ROOT_DIR}/utils/check_license/check-headers.sh
			${PMEMSTREAM_ROOT_DIR}
			BSD-3-Clause)

if(CHECK_CPP_STYLE)
	# XXX: update this, when clang format version is picked, and probably check min. version only
	find_program(CLANG_FORMAT NAMES clang-format) # XXX: add more versions and/or program names?
	set(CLANG_FORMAT_REQUIRED "9.0")
	if(CLANG_FORMAT)
		get_program_version_major_minor(${CLANG_FORMAT} CLANG_FORMAT_VERSION)
		message(STATUS "Found clang-format: ${CLANG_FORMAT} (version: ${CLANG_FORMAT_VERSION})")
		# XXX: check for VERSION_LESS only
		if(NOT (CLANG_FORMAT_VERSION VERSION_EQUAL CLANG_FORMAT_REQUIRED))
			message(FATAL_ERROR "required clang-format version is ${CLANG_FORMAT_REQUIRED}")
		endif()
	else()
		message(FATAL_ERROR "CHECK_CPP_STYLE=ON, but clang-format not found (required version: ${CLANG_FORMAT_REQUIRED})")
	endif()

	add_dependencies(checkers cppstyle)
endif()

if(DEVELOPER_MODE)
	# treat compiler warnings as errors
	if(WIN32)
		add_flag(-WX)
	else()
		add_flag(-Werror)

		# check for required programs for whitespace and license checks and add dependencies to ALL
		include(FindPerl)
		if(NOT PERL_FOUND)
			message(FATAL_ERROR "Perl not found")
		endif()
		if(PERL_VERSION_STRING VERSION_LESS 5.16)
			message(FATAL_ERROR "Too old Perl (<5.16)")
		endif()
		execute_process(COMMAND ${PERL_EXECUTABLE} -MText::Diff -e ""
						ERROR_QUIET
						RESULT_VARIABLE PERL_TEXT_DIFF_STATUS)
		if(PERL_TEXT_DIFF_STATUS)
			message(FATAL_ERROR "Text::Diff Perl module not found (install libtext-diff-perl or perl-Text-Diff)")
		endif()

		add_dependencies(checkers check-license)
		add_dependencies(checkers check-whitespace)
		add_check_whitespace(main ${PMEMSTREAM_ROOT_DIR}/CMakeLists.txt
						${PMEMSTREAM_ROOT_DIR}/cmake/*.*
						${PMEMSTREAM_ROOT_DIR}/*.*)
	endif()
endif()

# ----------------------------------------------------------------- #
## Add/include sub-directories (if build options enabled them)
# ----------------------------------------------------------------- #
add_subdirectory(src)

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(BUILD_DOC)
	add_subdirectory(doc)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

# ----------------------------------------------------------------- #
## Configure make install/uninstall and packages
# ----------------------------------------------------------------- #
# XXX: define these with first dependencies
# string(REPLACE ";" " " PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES}")
# string(REPLACE ";" ", " RPM_PACKAGE_REQUIRES "${RPM_DEPENDS}")
# string(REPLACE ";" ", " DEB_PACKAGE_REQUIRES "${DEB_DEPENDS}")

configure_file(${PMEMSTREAM_ROOT_DIR}/cmake/libpmemstream.pc.in
		${CMAKE_CURRENT_BINARY_DIR}/libpmemstream.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libpmemstream.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)

install(TARGETS pmemstream
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

configure_file(
	"${PMEMSTREAM_ROOT_DIR}/cmake/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# XXX: set it up within the source dir (and its own CMake file) ? do we need this?
# set up project's include dir (our source 'include' dir)
# include_directories(include)

# XXX: do we need to check for anything?
# Run checks for known issues, it's required to enable/disable some tests and examples.
# It has to be executed (included) here, when environment is fully set up,
# all packages are found and all paths/variables are set.
# include(${PMEMSTREAM_ROOT_DIR}/cmake/check_compiling_issues.cmake)

if(NOT "${CPACK_GENERATOR}" STREQUAL "")
	include(${PMEMSTREAM_ROOT_DIR}/cmake/packages.cmake)
endif()
