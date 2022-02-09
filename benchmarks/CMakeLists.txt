# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# benchmarks/CMakeLists.txt - CMake file for building all benchmarks along with
#	the current pmemstream sources
#

add_custom_target(benchmarks)

include_directories(${PMEMSTREAM_INCLUDE_DIRS} ${PMEMSTREAM_ROOT_DIR}/tests/common .)
link_directories(${PMEMSTREAM_LIBRARY_DIRS})
pkg_check_modules(LIBPMEMLOG REQUIRED libpmemlog)

function(add_benchmark name)
	set(srcs ${ARGN})
	prepend(srcs ${CMAKE_CURRENT_SOURCE_DIR} ${srcs})
	add_executable(benchmark-${name} ${srcs})
	add_dependencies(benchmarks benchmark-${name})
	target_link_libraries(benchmark-${name} ${LIBPMEM2_LIBRARIES} ${LIBPMEMLOG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} pmemstream)
	target_include_directories(benchmark-${name} PRIVATE ${PMEMSTREAM_ROOT_DIR}/src/include)
endfunction()

# ----------------------------------------------------------------- #
## Add developer checks
# ----------------------------------------------------------------- #
add_cppstyle(benchmarks ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]pp
	${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]pp)

add_check_whitespace(benchmarks ${CMAKE_CURRENT_SOURCE_DIR}/*.*
	${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]pp)

# ----------------------------------------------------------------- #
## Add benchmarks
# ----------------------------------------------------------------- #

add_benchmark(append append/main.cpp)
