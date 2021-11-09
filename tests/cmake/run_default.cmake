# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

include(${PARENT_SRC_DIR}/cmake/exec_functions.cmake)

setup()

execute(${TEST_EXECUTABLE} ${DIR}/testfile)

finish()
