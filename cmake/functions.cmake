# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021, Intel Corporation

#
# functions.cmake - generic helper functions for other CMake files
#

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# -------------------------------------------------------------------------- #
## Useful functions for string manipulations, flag setters and custom targets
# -------------------------------------------------------------------------- #

# join all ${VALUES} into an ${OUT} string, split with custom ${SEP}arator
function(join SEP OUT VALUES)
	string(REPLACE ";" "${SEP}" JOIN_TMP "${VALUES}")
	set(${OUT} "${JOIN_TMP}" PARENT_SCOPE)
endfunction()

# prepends prefix to list of strings
function(prepend var prefix)
	set(listVar "")
	foreach(f ${ARGN})
		list(APPEND listVar "${prefix}/${f}")
	endforeach(f)
	set(${var} "${listVar}" PARENT_SCOPE)
endfunction()

# Checks whether flag is supported by current C compiler
# and appends it to the relevant cmake variable, parameters:
# 1st: a compiler flag to add
# 2nd: (optional) a build type (DEBUG, RELEASE, ...), by default appends to common variable
macro(add_c_flag flag)
	string(REPLACE - _ flag2 ${flag})
	string(REPLACE " " _ flag2 ${flag2})
	string(REPLACE = "_" flag2 ${flag2})
	set(check_name "C_HAS_${flag2}")

	check_c_compiler_flag(${flag} ${check_name})

	if (${${check_name}})
		if (${ARGC} EQUAL 1)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
		else()
			set(CMAKE_C_FLAGS_${ARGV1} "${CMAKE_C_FLAGS_${ARGV1}} ${flag}")
		endif()
	endif()
endmacro()

# Checks whether flag is supported by current C++ compiler
# and appends it to the relevant cmake variable, parameters:
# 1st: a compiler flag to add
# 2nd: (optional) a build type (DEBUG, RELEASE, ...), by default appends to common variable
macro(add_cxx_flag flag)
	string(REPLACE - _ flag2 ${flag})
	string(REPLACE " " _ flag2 ${flag2})
	string(REPLACE = "_" flag2 ${flag2})
	set(check_name "CXX_HAS_${flag2}")

	check_cxx_compiler_flag(${flag} ${check_name})

	if (${${check_name}})
		if (${ARGC} EQUAL 1)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
		else()
			set(CMAKE_CXX_FLAGS_${ARGV1} "${CMAKE_CXX_FLAGS_${ARGV1}} ${flag}")
		endif()
	endif()
endmacro()

# Checks whether flag is supported by both C and C++ compiler and appends
# it to the relevant cmake variables.
# 1st argument is a flag
# 2nd (optional) argument is a build type (debug, release)
macro(add_common_flag flag)
	add_c_flag(${flag} ${ARGV1})
	add_cxx_flag(${flag} ${ARGV1})
endmacro()

# Add sanitizer flag, if it is supported, for C compiler
# XXX: perhaps we should also extend it for CXX compiler
macro(add_sanitizer_flag flag)
	set(SAVED_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
	set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} -fsanitize=${flag}")

	if(${flag} STREQUAL "address")
		set(check_name "C_HAS_ASAN")
	elseif(${flag} STREQUAL "undefined")
		set(check_name "C_HAS_UBSAN")
	endif()

	check_c_compiler_flag("-fsanitize=${flag}" ${check_name})
	if (${${check_name}})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${flag}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${flag}")
	else()
		message(STATUS "  ${flag} sanitizer is not supported")
	endif()

	set(CMAKE_REQUIRED_LIBRARIES ${SAVED_CMAKE_REQUIRED_LIBRARIES})
endmacro()

# Generates cppstyle-$name and cppformat-$name targets and attaches them
# as dependencies of global "cppformat" target.
# cppstyle-$name target verifies C++ style of files in current source dir.
# cppformat-$name target reformats files in current source dir.
# If more arguments are used, then they are used as files to be checked
# instead.
# ${name} must be unique.
function(add_cppstyle name)
	if(NOT CLANG_FORMAT OR (CLANG_FORMAT_VERSION VERSION_LESS CLANG_FORMAT_REQUIRED))
		return()
	endif()

	if(${ARGC} EQUAL 1)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cppstyle-${name}-status
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
				${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
			COMMAND ${PERL_EXECUTABLE}
				${PMEMSTREAM_ROOT_DIR}/utils/cppstyle
				${CLANG_FORMAT}
				check
				${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
				${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/cppstyle-${name}-status
			)

		add_custom_target(cppformat-${name}
			COMMAND ${PERL_EXECUTABLE}
				${PMEMSTREAM_ROOT_DIR}/utils/cppstyle
				${CLANG_FORMAT}
				format
				${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
				${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
			)
	else()
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cppstyle-${name}-status
			DEPENDS ${ARGN}
			COMMAND ${PERL_EXECUTABLE}
				${PMEMSTREAM_ROOT_DIR}/utils/cppstyle
				${CLANG_FORMAT}
				check
				${ARGN}
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/cppstyle-${name}-status
			)

		add_custom_target(cppformat-${name}
			COMMAND ${PERL_EXECUTABLE}
				${PMEMSTREAM_ROOT_DIR}/utils/cppstyle
				${CLANG_FORMAT}
				format
				${ARGN}
			)
	endif()

	add_custom_target(cppstyle-${name}
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/cppstyle-${name}-status)

	add_dependencies(cppstyle cppstyle-${name})
	add_dependencies(cppformat cppformat-${name})
endfunction()

# Generates check-whitespace-$name target and attaches it as a dependency
# of global "check-whitespace" target.
# ${name} must be unique.
function(add_check_whitespace name)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/check-whitespace-${name}-status
		DEPENDS ${ARGN}
		COMMAND ${PERL_EXECUTABLE}
			${PMEMSTREAM_ROOT_DIR}/utils/check_whitespace ${ARGN}
		COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/check-whitespace-${name}-status)

	add_custom_target(check-whitespace-${name}
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/check-whitespace-${name}-status)
	add_dependencies(check-whitespace check-whitespace-${name})
endfunction()

# ----------------------------------------------------------------- #
## Dependencies helpers
# ----------------------------------------------------------------- #

# Sets ${ret} to version of program specified by ${name} in major.minor format
function(get_program_version_major_minor name ret)
	execute_process(COMMAND ${name} --version
		OUTPUT_VARIABLE cmd_ret
		ERROR_QUIET)
	STRING(REGEX MATCH "([0-9]+.)([0-9]+)" VERSION ${cmd_ret})
	SET(${ret} ${VERSION} PARENT_SCOPE)
endfunction()

# ----------------------------------------------------------------- #
## Top-level CMake helpers
# ----------------------------------------------------------------- #

# src version shows the current version, as reported by git describe
# unless git is not available, then it's set to the recently released VERSION
function(set_source_ver SRCVERSION)
	# if there's version file committed, use it
	set(VERSION_FILE ${PMEMSTREAM_ROOT_DIR}/VERSION)
	if(EXISTS ${VERSION_FILE})
		file(STRINGS ${VERSION_FILE} FILE_VERSION)
		set(SRCVERSION ${FILE_VERSION} PARENT_SCOPE)
		return()
	endif()

	# otherwise take it from git
	execute_process(COMMAND git describe
		OUTPUT_VARIABLE GIT_VERSION
		WORKING_DIRECTORY ${PMEMSTREAM_ROOT_DIR}
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)
	if(GIT_VERSION)
		# 1.5-rc1-19-gb8f78a329 -> 1.5-rc1.git19.gb8f78a329
		string(REGEX MATCHALL
			"([0-9.]*)-rc([0-9]*)-([0-9]*)-([0-9a-g]*)"
			MATCHES
			${GIT_VERSION})
		if(MATCHES)
			set(SRCVERSION
				"${CMAKE_MATCH_1}-rc${CMAKE_MATCH_2}.git${CMAKE_MATCH_3}.${CMAKE_MATCH_4}"
				PARENT_SCOPE)
			return()
		endif()

		# 1.5-19-gb8f78a329 -> 1.5-git19.gb8f78a329
		string(REGEX MATCHALL
			"([0-9.]*)-([0-9]*)-([0-9a-g]*)"
			MATCHES
			${GIT_VERSION})
		if(MATCHES)
			set(SRCVERSION
				"${CMAKE_MATCH_1}-git${CMAKE_MATCH_2}.${CMAKE_MATCH_3}"
				PARENT_SCOPE)
			return()
		endif()
	else()
		execute_process(COMMAND git log -1 --format=%h
			OUTPUT_VARIABLE GIT_COMMIT
			WORKING_DIRECTORY ${PMEMSTREAM_ROOT_DIR}
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		set(SRCVERSION ${GIT_COMMIT} PARENT_SCOPE)

		# CPack may complain about commit sha being a package version
		if(NOT "${CPACK_GENERATOR}" STREQUAL "")
			message(WARNING "It seems this is a shallow clone. SRCVERSION is set to: \"${GIT_COMMIT}\". "
				"CPack may complain about setting it as a package version. Unshallow this repo before making a package.")
		endif()
		return()
	endif()

	# last chance: use version set up in the top-level CMake
	set(SRCVERSION ${VERSION} PARENT_SCOPE)
endfunction()
