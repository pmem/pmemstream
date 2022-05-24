# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

execute(${EXECUTABLE} a ${DIR}/testfile)
run_with_gdb(${TESTS_ROOT_DIR}/integrity/append_break_${TESTCASE}.gdb ${EXECUTABLE} b ${DIR}/testfile)
execute(${EXECUTABLE} y ${DIR}/testfile)

finish()
