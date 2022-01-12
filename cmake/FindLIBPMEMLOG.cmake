# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

message(STATUS "Checking for module 'libpmemlog' w/o PkgConfig")

find_library(LIBPMEMLOG_LIBRARY NAMES libpmemlog.so libpmemlog pmemlog)
set(LIBPMEMLOG_LIBRARIES ${LIBPMEMLOG_LIBRARY})

if(LIBPMEMLOG_LIBRARY)
	message(STATUS "  Found libpmemlog w/o PkgConfig")
else()
	set(MSG_NOT_FOUND "libpmemlog NOT found (set CMAKE_PREFIX_PATH to point the location)")
	if(LIBPMEMLOG_FIND_REQUIRED)
		message(FATAL_ERROR ${MSG_NOT_FOUND})
	else()
		message(WARNING ${MSG_NOT_FOUND})
	endif()
endif()
