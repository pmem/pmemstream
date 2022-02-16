# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

message(STATUS "Checking for module 'miniasync' w/o PkgConfig")

find_library(MINIASYNC_LIBRARY NAMES libminiasync.so libminiasync miniasync)
set(MINIASYNC_LIBRARIES ${MINIASYNC_LIBRARY})

if(MINIASYNC_LIBRARY)
	message(STATUS "  Found miniasync w/o PkgConfig")
else()
	set(MSG_NOT_FOUND "miniasync NOT found (set CMAKE_PREFIX_PATH to point the location)")
	if(MINIASYNC_FIND_REQUIRED)
		message(FATAL_ERROR ${MSG_NOT_FOUND})
	else()
		message(WARNING ${MSG_NOT_FOUND})
	endif()
endif()
