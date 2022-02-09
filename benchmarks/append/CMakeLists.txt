# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

cmake_minimum_required(VERSION 3.3)
project(benchmark-append)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPMEMSTREAM REQUIRED libpmemstream)
pkg_check_modules(LIBPMEMLOG REQUIRED libpmemlog)

include_directories(${LIBPMEMSTREAM_INCLUDE_DIRS} ../../tests/common . ..)
link_directories(${LIBPMEMSTREAM_LIBRARY_DIRS})

add_executable(benchmark-append main.cpp)
target_link_libraries(benchmark-append ${LIBPMEMSTREAM_LIBRARIES} ${LIBPMEMLOG_LIBRARIES} pthread)
