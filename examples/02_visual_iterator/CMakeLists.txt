# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

cmake_minimum_required(VERSION 3.3)
project(02_visual_iterator CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPMEMSTREAM REQUIRED libpmemstream)

include_directories(${LIBPMEMSTREAM_INCLUDE_DIRS} . ..)
link_directories(${LIBPMEMSTREAM_LIBRARY_DIRS})

add_executable(02_visual_iterator main.cpp)
target_link_libraries(02_visual_iterator ${LIBPMEMSTREAM_LIBRARIES})
