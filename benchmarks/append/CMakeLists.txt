# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

cmake_minimum_required(VERSION 3.3)
project(benchmark-append)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPMEMSTREAM REQUIRED libpmemstream)

include_directories(${LIBPMEMSTREAM_INCLUDE_DIRS} ../../tests/common . ..)
link_directories(${LIBPMEMSTREAM_LIBRARY_DIRS})

add_executable(benchmark-append main.cpp)
target_link_libraries(benchmark-append ${LIBPMEMSTREAM_LIBRARIES})
