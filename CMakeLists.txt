# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation

cmake_minimum_required(VERSION 3.16)

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
set(LIBPMEM2_REQUIRED_VERSION 1.10)
# Only pmreorder in ver. >= 1.9 guarantees reliable output
set(PMREORDER_REQUIRED_VERSION 1.9)

set(PKG_CONFIG_REQUIRES)
set(DEB_DEPENDS)
set(RPM_DEPENDS)

set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/tests CACHE STRING "working directory for tests")
message(STATUS "TEST_DIR set to: \"${TEST_DIR}\"")

# Do not treat include directories from the interfaces
# of consumed Imported Targets as SYSTEM by default.
set(CMAKE_NO_SYSTEM_FROM_IMPORTED 1)

# ----------------------------------------------------------------- #
## CMake build options
# ----------------------------------------------------------------- #
option(BUILD_EXAMPLES "build examples" ON)
option(BUILD_TESTS "build tests" ON)
option(BUILD_DOC "build documentation" ON)

option(USE_CCACHE "use ccache if it is available in the system" ON)
option(COVERAGE "enable collecting of coverage data" OFF)
option(DEVELOPER_MODE "enable developer checks" OFF)
option(CHECK_CPP_STYLE "check code style of C++ sources" OFF)
option(TRACE_TESTS "more verbose test outputs" OFF)
option(USE_LIBUNWIND "use libunwind for more reliable stack traces from tests (it's disabled when running on Valgrind)" ON)
option(USE_ASAN "enable AddressSanitizer checks" OFF)
option(USE_UBSAN "enable UndefinedBehaviorSanitizer checks" OFF)

option(TESTS_USE_FORCED_PMEM "run tests with PMEM_IS_PMEM_FORCE=1 - it speeds up tests execution on emulated pmem" OFF)
option(TESTS_USE_VALGRIND "enable tests with valgrind (fail build if Valgrind not found)" ON)
option(TESTS_PMREORDER "enable tests with pmreorder (if pmreorder found; it requires PMDK ver. >= 1.9)" OFF)
option(TESTS_LONG "enable long running tests" OFF)
option(TESTS_RAPIDCHECK "enable rapidcheck tests" ON)

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

# all our components require libpmem2
if(PKG_CONFIG_FOUND)
	pkg_check_modules(LIBPMEM2 REQUIRED libpmem2>=${LIBPMEM2_REQUIRED_VERSION})
else()
	find_package(LIBPMEM2 REQUIRED ${LIBPMEM2_REQUIRED_VERSION})
endif()
include_directories(${LIBPMEM2_INCLUDE_DIRS})
link_directories(${LIBPMEM2_LIBRARY_DIRS})

list(APPEND PKG_CONFIG_REQUIRES "libpmem2 >= ${LIBPMEM2_REQUIRED_VERSION}")
list(APPEND RPM_DEPENDS "libpmem2-devel >= ${LIBPMEM2_REQUIRED_VERSION}")
list(APPEND DEB_DEPENDS "libpmem2-dev(>= ${LIBPMEM2_REQUIRED_VERSION})")

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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -coverage")
endif()

add_common_flag(-Wall)
add_common_flag(-Wmissing-variable-declarations)
add_common_flag(-Wpointer-arith)
add_common_flag(-Wsign-conversion)
add_common_flag(-Wsign-compare)
add_common_flag(-Wunreachable-code-return)
add_common_flag(-Wunused-macros)
add_common_flag(-Wno-maybe-uninitialized)
add_common_flag(-fno-common)
add_common_flag(-ggdb DEBUG)
add_common_flag(-DDEBUG DEBUG)
add_common_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" RELEASE)

add_custom_target(checkers ALL)
add_custom_target(cppstyle)
add_custom_target(cppformat)
add_custom_target(check-whitespace)
add_custom_target(check-license
	COMMAND ${PMEMSTREAM_ROOT_DIR}/utils/check_license/check-headers.sh
			${PMEMSTREAM_ROOT_DIR}
			BSD-3-Clause -v)
add_custom_target(copyright-format
	COMMAND ${PMEMSTREAM_ROOT_DIR}/utils/check_license/check-headers.sh
			${PMEMSTREAM_ROOT_DIR}
			BSD-3-Clause -d)

if(CHECK_CPP_STYLE)
	# XXX: update this, when clang format version is picked, and probably check min. version only
	find_program(CLANG_FORMAT NAMES clang-format-9 clang-format-9.0 clang-format)
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
		add_common_flag(-WX)
	else()
		add_common_flag(-Werror)

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

if(BUILD_DOC)
	add_subdirectory(doc)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

# ----------------------------------------------------------------- #
## Configure make install/uninstall and packages
# ----------------------------------------------------------------- #
string(REPLACE ";" " " PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES}")
string(REPLACE ";" ", " RPM_PACKAGE_REQUIRES "${RPM_DEPENDS}")
string(REPLACE ";" ", " DEB_PACKAGE_REQUIRES "${DEB_DEPENDS}")

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

if(NOT "${CPACK_GENERATOR}" STREQUAL "")
	include(${PMEMSTREAM_ROOT_DIR}/cmake/packages.cmake)
endif()
