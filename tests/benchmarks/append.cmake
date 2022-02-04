# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

execute(${EXECUTABLE} --path ${DIR}/testfile-pmemstream)
execute(${EXECUTABLE} --engine pmemlog --path ${DIR}/testfile-pmemlog)

finish()
