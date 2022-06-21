# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# Run binary under pmreorder under pmreorder. ReorderPartial is used because ReorderAccumulative
# would take too much time

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)
set(EXPECT_SUCCESS true)
set(TEST_FILE ${DIR}/testfile)

setup()

message(STATUS "Running main test for ${EXECUTABLE}")

execute(${EXECUTABLE} create ${DIR}/testfile)
pmreorder_create_store_log(${DIR}/testfile ${EXECUTABLE} fill ${DIR}/testfile)

pmreorder_execute(${EXPECT_SUCCESS} ReorderPartial ${SRC_DIR}/integrity/pmreorder.conf
       ${CMAKE_COMMAND}
       \\-DTESTS_ROOT_DIR=${TESTS_ROOT_DIR}
       \\-DEXECUTABLE=${EXECUTABLE}
       \\-DTEST_FILE=${TEST_FILE}
       \\-DBIN_DIR=${BIN_DIR}
       \\-DSRC_DIR=${SRC_DIR}
       \\-DTEST_NAME=${TEST_NAME}_SUBTEST
       \\--verbose
       \\-P ${TESTS_ROOT_DIR}/integrity/crash_during_recovery_subtest.cmake)

finish()
