# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation

add_cppstyle(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[chp]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/common/*.[chp])

add_check_whitespace(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[chp]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/common/*.[chp])

set(SOURCES libpmemstream.c)

add_library(pmemstream SHARED ${SOURCES})

target_include_directories(pmemstream PRIVATE
	.
	include)

target_link_libraries(pmemstream PRIVATE
	-Wl,--version-script=${PMEMSTREAM_ROOT_DIR}/src/libpmemstream.map
	${LIBPMEM2_LIBRARIES})

set_target_properties(pmemstream PROPERTIES
	SOVERSION 0
	PUBLIC_HEADER "src/include/libpmemstream.h")

target_compile_definitions(pmemstream PRIVATE SRCVERSION="${SRCVERSION}")

# valgrind
# if(VALGRIND_FOUND)
# 	target_include_directories(pmemstream PRIVATE src/valgrind)
# endif()
