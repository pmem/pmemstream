# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

execute(${EXECUTABLE} ${DIR}/testfile)

finish()
