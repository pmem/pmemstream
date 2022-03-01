# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

cmake_minimum_required(VERSION 3.3)
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME} C)

include(FindThreads)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPMEMSTREAM REQUIRED libpmemstream)

include_directories(${LIBPMEMSTREAM_INCLUDE_DIRS} . ..)
link_directories(${LIBPMEMSTREAM_LIBRARY_DIRS})

set(SRC main.c
	ringbuffer.c)

add_executable(${CMAKE_PROJECT_NAME} ${SRC})
target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBPMEMSTREAM_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
