# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

# this run cmake file is useful e.g. for running examples

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

# create 100MiB file
execute_process(COMMAND dd if=/dev/zero of=${DIR}/testfile bs=1024 count=102400)
execute(${EXECUTABLE} ${DIR}/testfile)

finish()
