# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)
set(EXPECT_SUCCESS true)

setup()

execute(${EXECUTABLE} create ${DIR}/pretest_testfile)
execute(${EXECUTABLE} fill ${DIR}/pretest_testfile)
execute(${EXECUTABLE} check ${DIR}/pretest_testfile)

execute(${EXECUTABLE} create ${DIR}/testfile)
pmreorder_create_store_log(${DIR}/testfile ${EXECUTABLE} fill ${DIR}/testfile)
pmreorder_execute(${EXPECT_SUCCESS} ReorderAccumulative ${SRC_DIR}/integrity/pmreorder.conf ${EXECUTABLE} check)

finish()
