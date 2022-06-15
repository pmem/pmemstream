# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

# init example stream
execute(${CMAKE_BINARY_DIR}/../examples/example-01_basic_iterate ${DIR}/testfile)
# ... and run another example/test
execute(${EXECUTABLE} ${DIR}/testfile)

finish()
