# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# examples/CMakeLists.txt - CMake file for building all examples along with
#	the current pmemstream sources, for verification purposes.
#	To build an example as a standalone application (with pmemstream from the system)
#	see a CMake file in any of the subdirectories.
#
add_custom_target(examples)

include_directories(.)

# ----------------------------------------------------------------- #
## Setup examples
# ----------------------------------------------------------------- #
add_dependencies(examples pmemstream)

function(add_example name)
	set(srcs ${ARGN})
	prepend(srcs ${CMAKE_CURRENT_SOURCE_DIR} ${srcs})
	add_executable(example-${name} ${srcs})
	add_dependencies(examples example-${name})
    target_link_libraries(example-${name} pmemstream)
	target_include_directories(example-${name} PRIVATE ${PMEMSTREAM_ROOT_DIR}/src/include)
endfunction()

# ----------------------------------------------------------------- #
## Add developer checks
# ----------------------------------------------------------------- #
add_cppstyle(examples ${CMAKE_CURRENT_SOURCE_DIR}/01-iterate/*.c)

add_check_whitespace(examples ${CMAKE_CURRENT_SOURCE_DIR}/01-iterate/*.*)

# ----------------------------------------------------------------- #
## Add examples
# ----------------------------------------------------------------- #
add_example(01-iterate 01-iterate/main.c)
target_link_libraries(example-01-iterate ${LIBPMEM2_LIBRARIES})

add_example(02-visual_iterator 02-visual_iterator/main.cpp)
target_link_libraries(example-02-visual_iterator ${LIBPMEM2_LIBRARIES})
