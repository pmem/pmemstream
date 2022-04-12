# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

cmake_minimum_required(VERSION 3.3)
project(03_reserve_publish CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPMEMSTREAM REQUIRED libpmemstream)

include_directories(${LIBPMEMSTREAM_INCLUDE_DIRS} . ..)
link_directories(${LIBPMEMSTREAM_LIBRARY_DIRS})

add_executable(05_timestamp_based_order main.cpp)
target_link_libraries(05_timestamp_based_order ${LIBPMEMSTREAM_LIBRARIES} pthread)
