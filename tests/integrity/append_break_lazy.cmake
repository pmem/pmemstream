# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

execute(${EXECUTABLE} init ${DIR}/testfile)
run_with_gdb(${TESTS_ROOT_DIR}/integrity/append_break_${TESTCASE}.gdb ${EXECUTABLE} break ${DIR}/testfile)
execute(${EXECUTABLE} iterate_after ${DIR}/testfile)

finish()
