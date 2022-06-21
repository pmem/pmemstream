# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)
set(EXPECT_SUCCESS true)

set(SUBTEST_FILE ${TEST_FILE}.subtest)

message(STATUS "Running Subtest")

# Pmreorder may alter test file, so copy is needed to not interfere with top level pmreorder instance
configure_file(${TEST_FILE} ${SUBTEST_FILE} COPYONLY)

pmreorder_create_store_log(${SUBTEST_FILE} ${EXECUTABLE} check ${SUBTEST_FILE})
pmreorder_execute(${EXPECT_SUCCESS} ReorderPartial ${SRC_DIR}/integrity/pmreorder.conf ${EXECUTABLE} check)

# Make sure there is no interference between subtests.
file(REMOVE ${SUBTEST_FILE})

